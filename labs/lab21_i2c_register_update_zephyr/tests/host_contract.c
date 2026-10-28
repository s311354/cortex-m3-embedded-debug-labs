#include "reg_access.h"

#include <zephyr/drivers/i2c.h>

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(UINT8_MAX == 255 && UINT16_MAX == 65535, "This lab tests eight-bit registers and 16-bit address fields");

enum {
    BUS_COUNT = 2,
    ADDRESS_COUNT = 128,
    REGISTER_COUNT = 256
};

enum call_kind {
    NO_CALLS,
    READ_BYTE,
    WRITE_BYTE,
    UPDATE_BYTE
};

static const char *group = "startup";
static uint64_t cases, assertions;
static unsigned int detail_old, detail_mask, detail_value;

#define CHECK(expr) do { \
	++assertions; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL [%s] %s:%d %s " \
				"(old=%u mask=%u value=%u)\n", \
			       	group, __FILE__, __LINE__, #expr, \
				detail_old, detail_mask, detail_value); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device buses[BUS_COUNT] = {{true}, {true}};
static uint8_t registers[BUS_COUNT][ADDRESS_COUNT][REGISTER_COUNT];

static struct {
    enum call_kind kind;
    const struct device *bus;
    uint16_t addr;
    uint8_t reg, write_value;
    unsigned int reads, writes;
    char trace[2];
    size_t trace_len;
    int read_error, write_error;
    bool modify_read_on_error, commit_write_on_error;
    bool unready_after_read;
} mock;

static void arm(enum call_kind kind, const struct i2c_client *client, uint8_t reg, uint8_t write_value) {
    memset(&mock, 0, sizeof(mock));
    mock.kind = kind;
    mock.bus = client == NULL ? NULL : client->bus;
    mock.addr = client == NULL ? 0U : client->addr;
    mock.reg = reg;
    mock.write_value = write_value;
}

static unsigned int bus_index(const struct device *dev) {
    if (dev == &buses[0]) {
        return 0U;
    }

    if (dev == &buses[1]) {
        return 1U;
    }

    CHECK(false);
    return 0U;
}

static void record_call(char kind) {
    CHECK(mock.trace_len < sizeof(mock.trace));
    mock.trace[mock.trace_len++] = kind;
}

static void expect_calls(unsigned int reads, unsigned int writes) {
    CHECK(mock.reads == reads);
    CHECK(mock.writes == writes);
    CHECK(mock.trace_len == (size_t)(reads + writes));

    if (reads != 0U) {
        CHECK(mock.trace[0] == 'R');
    }

    if (writes != 0U) {
        CHECK(mock.trace[reads] == 'W');
    }
}

int i2c_write_read(const struct device *dev, uint16_t addr, const void *write_buf, size_t num_write, 
		   void *read_buf, size_t num_read) {
    CHECK(mock.kind == READ_BYTE || mock.kind == UPDATE_BYTE);
    CHECK(dev == mock.bus && dev != NULL && dev->ready);
    CHECK(addr == mock.addr && addr < ADDRESS_COUNT);
    CHECK(write_buf != NULL && read_buf != NULL);
    CHECK(num_write == 1U && num_read == 1U);
    CHECK(*(const uint8_t *)write_buf == mock.reg);
    CHECK(mock.reads == 0U && mock.writes == 0U);

    ++mock.reads;
    record_call('R');

    const unsigned int bank = bus_index(dev);

    if (mock.read_error == 0 || mock.modify_read_on_error) {
        *(uint8_t *) read_buf = registers[bank][addr][mock.reg];
    }

    if (mock.unready_after_read) {
        buses[bank].ready = false;
    }

    return mock.read_error;
}

int i2c_write(const struct device *dev, const uint8_t *buf, uint32_t num_bytes, uint16_t addr) {
    CHECK(mock.kind == WRITE_BYTE || mock.kind == UPDATE_BYTE);
    CHECK(dev == mock.bus && dev != NULL && dev->ready);
    CHECK(addr == mock.addr && addr < ADDRESS_COUNT);
    CHECK(buf != NULL && num_bytes == 2U);
    CHECK(buf[0] == mock.reg && buf[1] == mock.write_value);
    CHECK(mock.writes == 0U);
    CHECK(mock.reads == (mock.kind == UPDATE_BYTE ? 1U : 0U));

    ++mock.writes;
    record_call('W');

    if (mock.write_error == 0 || mock.commit_write_on_error) {
        registers[bus_index(dev)][addr][buf[0]] = buf[1];
    }

    return mock.write_error;
}

static void check_client(const struct i2c_client *client, const struct device *bus, uint16_t addr) {
    CHECK(client->bus == bus && client->addr == addr);
}

/* Independent bit-by-bit oracle */
static uint8_t expected_update(uint8_t old, uint8_t mask, uint8_t value) {
    unsigned int result = 0U;

    for (unsigned int bit = 0U; bit < 8U; ++bit) {
        const unsigned int source = ((mask >> bit) & 1U) ? value : old;

	if (((source >> bit) & 1U) != 0U) {
	    result |= 1U << bit;
	}
    }

    return   (uint8_t) result;
}

static void reject_client(const struct i2c_client *client, int error) {
    uint8_t output[3] = {0xa5U, 0xccU, 0x5aU};

    arm(NO_CALLS, client, 0x10U, 0U);

    CHECK(reg_read_byte(client, 0x10U, &output[1]) == error);
    CHECK(output[0] == 0xa5U && output[1] == 0xccU && output[2] == 0x5aU);
    CHECK(reg_write_byte(client, 0x10U, 0xffU) == error);
    CHECK(reg_update_byte(client, 0x10U, 0xffU, 0xa5U) == error);
    CHECK(reg_update_byte(client, 0x10U, 0U, 0xa5U) == error);
    expect_calls(0U, 0U);
    ++cases;
}

static void validation(void) {
    struct i2c_client client = {&buses[0], 0x2aU};

    reject_client(NULL, -EINVAL);

    client.bus = NULL;
    reject_client(&client, -EINVAL);

    client.bus = &buses[0];
    buses[0].ready = false;
    reject_client(&client, -ENODEV);

    /* NULL output is rejected before client readiness is queried */
    arm(NO_CALLS, &client, 0U, 0U);

    CHECK(reg_read_byte(&client, 0U, NULL) == -EINVAL);
    CHECK(reg_read_byte(NULL, 0U, NULL) == -EINVAL);
    expect_calls(0U, 0U);
    ++cases;

    /* Entire out-of-range uint16_t address space, including not-ready cases */
    for (unsigned int addr = 128U; addr <= UINT16_MAX; ++addr) {
        buses[0].ready = (addr & 1U) != 0U;
	client.addr = (uint16_t) addr;
	reject_client(&client, -EINVAL);
	check_client(&client, &buses[0], (uint16_t)addr);
    }

    buses[0].ready = true;
}

static void read_matrix(void) {
    for (unsigned int reg = 0U; reg < 256U; ++reg) {
        for (unsigned int value = 0U; value < 256U; ++value) {
	    const unsigned int bank = reg & 1U;
	    const uint16_t addr = (uint16_t)((reg + value) & 0x7fU);

	    const struct i2c_client client = { 
		    &buses[bank],
		    addr
	    };

	    uint8_t output[3] = {0xa5U, (uint8_t)(value ^ 0xffU), 0x5aU};

	    detail_value = value;
	    registers[bank][addr][reg] = (uint8_t) value;

	    arm(READ_BYTE, &client, (uint8_t)reg, 0U);

	    CHECK(reg_read_byte(&client, (uint8_t)reg, &output[1]) == 0);
	    CHECK(output[0] == 0xa5U && output[2] == 0x5aU);
	    CHECK(output[1] == value && registers[bank][addr][reg] == value);
	    expect_calls(1U, 0U);
	    check_client(&client, &buses[bank], addr);
	    ++cases;
	}
    }
}


static void write_matrix(void) {
    for (unsigned int reg = 0U; reg < 256U; ++reg) {
        for (unsigned int value = 0U; value < 256U; ++value) {
	    const unsigned int bank = reg & 1U;
	    const uint16_t addr = (uint16_t) ((reg + value) & 0x7fU);
	    
	    const struct i2c_client client = {
	        &buses[bank],
		addr
	    };

	    detail_value = value;
	    registers[bank][addr][reg] = (uint8_t) (value ^ 0xffU);

	    arm(WRITE_BYTE, &client, (uint8_t)reg, (uint8_t)value);

	    CHECK(reg_write_byte(&client, (uint8_t)reg, (uint8_t)value) == 0);
	    CHECK(registers[bank][addr][reg] == value);
	    expect_calls(0U, 1U);
	    check_client(&client, &buses[bank], addr);
	    ++cases;
	}
    }
}

static void update_matrix(void) {
    for (unsigned int old = 0U; old < 256U; ++old) {
        detail_old = old;

	for (unsigned int mask = 0U; mask < 256U; ++mask) {
	    detail_mask = mask;
	    const unsigned int bank = old & 1U;
	    const uint16_t addr = (uint16_t) (mask & 0x7fU);

	    const struct i2c_client client = {
	        &buses[bank],
		addr
	    };

	    for (unsigned int value = 0U; value < 256U; ++value) {
	        detail_value = value;
		const uint8_t reg = (uint8_t) (old ^ mask ^ value);
		const uint8_t expected = expected_update((uint8_t)old, (uint8_t)mask, (uint8_t)value);

		registers[bank][addr][reg] = (uint8_t) old;

		arm(UPDATE_BYTE, &client, reg, expected);

		CHECK(reg_update_byte(&client, reg, (uint8_t)mask, (uint8_t)value) == 0);
		CHECK(registers[bank][addr][reg] == expected);
		CHECK((expected & (uint8_t)~mask) == (old & (uint8_t)~mask));
		CHECK((expected & mask) == (value & mask));
		expect_calls(1U, expected != old ? 1U : 0U);
		check_client(&client, &buses[bank], addr);
		++cases;
	    }
	}
    }
}

static void idempotence_and_fresh_read(void) {
    const struct i2c_client client = {
        &buses[0],
        0x2aU	
    };

    for (unsigned int reg = 0U; reg < 256U; ++reg) {
        for (unsigned int mask = 0U; mask < 256U; ++mask) {
	    const uint8_t old = (uint8_t) (reg + mask);
	    const uint8_t value = (uint8_t) (reg ^ (mask * 37U));
	    const uint8_t desired = expected_update(old, (uint8_t)mask, value);
	    registers[0][client.addr][reg] = old;

	    arm(UPDATE_BYTE, &client, (uint8_t)reg, desired);

	    CHECK(reg_update_byte(&client, (uint8_t)reg, (uint8_t)mask, value) == 0);
	    expect_calls(1U, desired != old ? 1U : 0U);

	    /* Same update is a read-only no-op, even with a write error armed */
	    arm(UPDATE_BYTE, &client, (uint8_t)reg, desired);

	    mock.write_error = -EIO;
	    CHECK(reg_update_byte(&client, (uint8_t)reg, (uint8_t)mask, value) == 0);
	    expect_calls(1U, 0U);
	    CHECK(registers[0][client.addr][reg] == desired);

	    /* a later device change must not be hidden by a stale cache */
	    const uint8_t changed = (uint8_t) ~desired;
	    const uint8_t fresh = expected_update(changed, (uint8_t)mask, value);
	    registers[0][client.addr][reg] = changed;
	    
	    arm(UPDATE_BYTE, &client, (uint8_t)reg, fresh);

	    CHECK(reg_update_byte(&client, (uint8_t)reg, (uint8_t)mask, value) == 0);
	    expect_calls(1U, fresh != changed ? 1U : 0U);
	    CHECK(registers[0][client.addr][reg] == fresh);
	    ++cases;
	}
    }
}

static void error_paths(void) {
    const int errors[] = {-EIO, -ETIMEDOUT, -ENXIO, -EAGAIN, -EBUSY, -ENODEV};
    const struct i2c_client client = {
        &buses[0],
	0x2aU
    };
    const uint8_t reg = 0x10U;
    const uint8_t old = 0x55U;
    const uint8_t value = 0xaaU;

    for (size_t e = 0U; e < sizeof(errors)/sizeof(errors[0]); ++e) {
	for (unsigned int touched = 0U; touched < 2U; ++touched ) {
            registers[0][client.addr][reg] = old;
  	    uint8_t output[3] = {0xa5U, 0xccU, 0x5aU};
  
  	    arm(READ_BYTE, &client, reg, 0U);
  
  	    mock.read_error = errors[e];
  	    mock.modify_read_on_error = touched != 0U;
  
  	    CHECK(reg_read_byte(&client, reg, &output[1]) == errors[e]);
  	    CHECK(output[0] == 0xa5U && output[2] == 0x5aU);
  	    CHECK(output[1] == (touched ? old : 0xccU));
  	    expect_calls(1U, 0U);
  	    ++cases;


	    for (unsigned int m = 0U; m < 2U; ++m) {
                const int8_t mask = m ? 0xffU : 0U;

		arm(UPDATE_BYTE, &client, reg, value);

		mock.read_error = errors[e];
		mock.modify_read_on_error = touched != 0U;

		CHECK(reg_update_byte(&client, reg, mask, value) == errors[e]);
		CHECK(registers[0][client.addr][reg] == old);
		expect_calls(1U, 0U);
		++cases;
	    }

	    for (unsigned int update = 0U; update < 2U; ++update) {
	        registers[0][client.addr][reg] = old;

		arm(update ? UPDATE_BYTE : WRITE_BYTE, &client, reg, value);

		mock.write_error = errors[e];
		mock.commit_write_on_error = touched != 0U;

		const int rc = update ? 
			       reg_update_byte(&client, reg, 0xffU, value) :
			       reg_write_byte(&client, reg, value);

		CHECK(rc == errors[e]);
		CHECK(registers[0][client.addr][reg] == (touched ? value : old));
		expect_calls(update, 1U);
		check_client(&client, &buses[0], client.addr);

		/* Errors are propagated, not latched */
		arm(READ_BYTE, &client, reg, 0U);

                uint8_t readback = 0U;
		CHECK(reg_read_byte(&client, reg, &readback) == 0);
		CHECK(readback == (touched ? value : old));
		expect_calls(1U, 0U);
		++cases;
	    }
	}
    }

    for (unsigned int changed = 0U; changed < 2U; ++changed) {
        registers[0][client.addr][reg] = old;

	arm(UPDATE_BYTE, &client, reg, value);

	mock.unready_after_read = true;
	CHECK(reg_update_byte(&client, reg, changed ? 0xffU : 0U, value) == (changed ? -ENODEV : 0));
	expect_calls(1U, 0U);
	buses[0].ready = true;
	++cases;
    }
}

static void client_and_register_isolation(void) {
    static uint8_t expected[BUS_COUNT][ADDRESS_COUNT][REGISTER_COUNT];

    for (unsigned int b = 0U; b < BUS_COUNT; ++b) {
        for (unsigned int a = 0U; a < ADDRESS_COUNT; ++a) {
	    for (unsigned int r = 0U; r < REGISTER_COUNT; ++r) {
	        registers[b][a][r] = (uint8_t)(b * 71U + a * 17U + r) ;
	    }
	}
    }

    memcpy(expected, registers, sizeof(expected));

    for (unsigned int a = 0U; a < ADDRESS_COUNT; ++a) {
        for (unsigned int b = 0U; b < BUS_COUNT; ++b) {
	    const struct i2c_client client = {
	        &buses[b],
		(uint16_t) a
	    };

	    const uint8_t r = (uint8_t) (a * 29U);
	    const uint8_t v = (uint8_t) ~expected[b][a][r];

	    arm(WRITE_BYTE, &client, r, v);

	    CHECK(reg_write_byte(&client, r, v) == 0);
	    expect_calls(0U, 1U);
	    expected[b][a][r] = v;

	    CHECK(memcmp(registers, expected, sizeof(expected)) == 0);

	    /* Interleave another bus with the same address and register */
	    const unsigned int other = b ^ 1U;

	    const struct i2c_client peer = { 
	        &buses[other],
		(uint16_t) a
	    };

	    arm(READ_BYTE, &peer, r, 0U);

	    uint8_t out = 0U;
	    CHECK(reg_read_byte(&peer, r, &out) == 0);
	    CHECK(out == expected[other][a][r]);
	    expect_calls(1U, 0U);

	    const uint8_t masked = expected_update(v, 0x5aU, 0xa5U);

	    arm(UPDATE_BYTE, &client, r, masked);

	    CHECK(reg_update_byte(&client, r, 0x5aU, 0xa5U) == 0);
	    expect_calls(1U, masked != v ? 1U : 0U);
	    expected[b][a][r] = masked;

	    CHECK(memcmp(registers, expected, sizeof(expected)) == 0);
	    check_client(&client, &buses[b], (uint16_t)a);
	    check_client(&peer, &buses[other], (uint16_t)a);
	    ++cases;
	}
    }
}

static void run_group(const char *name, void (*test)(void)) {
    const uint64_t before = cases;
    group = name;
    detail_old = detail_mask = detail_value = 0U;
    test();
    printf("PASS [%s]: %" PRIu64 " cases\n", name, cases - before);
}

int main(void) {
    run_group("argument, readiness and all invalid addresses", validation);
    run_group("all register/value reads and output bounds", read_matrix);
    run_group("all register/value writes", write_matrix);
    run_group("driver errors and readiness changes", error_paths);
    run_group("idempotence and fresh reads", idempotence_and_fresh_read);
    run_group("bus/address/register isolation", client_and_register_isolation);
    run_group("all 256^3 old/mask/value combinations", update_matrix);
    printf("PASS: %" PRIu64 " cases; %" PRIu64 " assertions\n", cases, assertions);
    puts("Host I2C register-helper contract only; not Zephyr, an I2C waveform, "
 	 "EEPROM/flash persistence, or hardware verification.");
    return 0;
}
