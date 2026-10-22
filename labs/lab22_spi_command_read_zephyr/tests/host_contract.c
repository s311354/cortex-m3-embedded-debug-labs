#include "spi_command.h"

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(SPI_COMMAND_MAX_REPLY == 16U, "Update the Lab22 contract fixtures if the reply limit changes");

static const char *group = "startup";
static uint64_t cases, checks;

#define CHECK(expr) do { \
	++checks; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL [%s] case=%" PRIu64 " %s:%d %s\n", group, cases + 1U, __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device bus = {true}, gpio = {true};
static struct spi_config config;
static struct spi_client client;

enum profile {
    LOOPBACK,
    GPIO_LOW,
    GPIO_HIGH,
    PROFILE_COUNT
};

struct mock_transfer {
    bool armed;
    const struct device *bus;
    const struct spi_config *config;
    uint8_t command;
    uint8_t *reply;
    size_t reply_len;
    size_t wire_limit;
    int result;
    unsigned int calls;
    size_t frames, discarded;
    uint8_t tx_wire[SPI_COMMAND_MAX_REPLY + 1U];
    uint8_t rx_wire[SPI_COMMAND_MAX_REPLY + 1U];
};

static struct mock_transfer mock;

static void configure(enum profile profile, unsigned int mode) {
    memset(&config, 0, sizeof(config));
    memset(&mock, 0, sizeof(mock));

    bus.ready = gpio.ready = true;

    config.frequency = 781250U;
    config.operation = (spi_operation_t)(SPI_WORD_SET(8) |
	               ((mode & 2U) ? SPI_MODE_CPOL : 0U) |
		       ((mode & 1U) ? SPI_MODE_CPHA : 0U));

    /* Nonzero unrelated fields exercise unchanged forwarding */
    config.peripheral = 7U;
    config.word_delay = 19U;

    if (profile == LOOPBACK) {
        config.operation |= SPI_MODE_LOOP;
    } else {
        config.cs.cs_is_gpio = true;
	config.cs.gpio.port = &gpio;
	config.cs.gpio.pin = 11U;
	config.cs.delay = 3U;
	config.cs.gpio.dt_flags = profile == GPIO_LOW ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;

	if (profile == GPIO_HIGH)
	    config.operation |= SPI_CS_ACTIVE_HIGH;
    }

    client.bus = &bus;
    client.config = &config;
}

/* Artificial command-aware fixture */
static uint8_t reply_byte(uint8_t command, size_t index) {
    static const uint8_t sample[] = {0xefU, 0x40U, 0x18U};

    if (command == 0x9fU && index < sizeof(sample)) {
        return sample[index];
    }

    return (uint8_t)((((uint32_t) command * 37U) + (uint32_t) index * 29U) ^ 0xa5U);
}

struct cursor {
    const struct spi_buf_set *set;
    size_t descriptor, offset;
};

static const struct spi_buf *current_buffer(struct cursor *cursor) {
    while (cursor->descriptor < cursor->set->count && 
	   cursor->offset == cursor->set->buffers[cursor->descriptor].len) {
        ++cursor->descriptor;
	cursor->offset = 0U;
    }

    CHECK(cursor->descriptor < cursor->set->count);

    return &cursor->set->buffers[cursor->descriptor];
}

static uint8_t clock_tx(struct cursor *cursor) {
    const struct spi_buf *buffer = current_buffer(cursor);
    const uint8_t value = buffer->buf == NULL ? 0U : ((const uint8_t *)buffer->buf)[cursor->offset];

    ++cursor->offset;
    return value;
}

static void clock_rx(struct cursor *cursor, uint8_t value) {
    const struct spi_buf *buffer = current_buffer(cursor);

    if (buffer->buf != NULL) {
        ((uint8_t *) buffer->buf)[cursor->offset] = value;
    } else {
        ++mock.discarded;
    }

    ++cursor->offset;
}

int spi_transceive(const struct device *dev, const struct spi_config *cfg, 
		   const struct spi_buf_set *tx, const struct spi_buf_set *rx) {
    ++mock.calls;

    CHECK(mock.armed && mock.calls == 1U);
    CHECK(dev == mock.bus && cfg == mock.config);
    CHECK(tx != NULL && rx != NULL);
    CHECK(tx->buffers != NULL && rx->buffers != NULL);
    CHECK(tx->count == 2U && rx->count == 2U);

    /* Deliberately check the supplied implementation's 2+2 layout */
    CHECK(*(const uint8_t *)tx->buffers[0].buf == mock.command);
    CHECK(tx->buffers[0].buf != NULL && tx->buffers[0].len == 1U);
    CHECK(tx->buffers[1].buf == NULL && tx->buffers[1].len == mock.reply_len);

    CHECK(rx->buffers[0].buf == NULL && rx->buffers[0].len == 1U);
    CHECK(rx->buffers[1].buf == mock.reply && rx->buffers[1].len == mock.reply_len);

    CHECK(mock.reply_len >= 1U && mock.reply_len <= SPI_COMMAND_MAX_REPLY);
    CHECK(tx->buffers[0].len + tx->buffers[1].len == 1U + mock.reply_len);
    CHECK(rx->buffers[0].len + rx->buffers[1].len == 1U + mock.reply_len);
    CHECK(mock.wire_limit <= 1U + mock.reply_len);

    struct cursor tx_cursor = {tx, 0U, 0U};
    struct cursor rx_cursor = {rx, 0U, 0U};

    for (size_t position = 0U; position < mock.wire_limit; ++position) {
        const uint8_t transmitted = clock_tx(&tx_cursor);

	CHECK(transmitted == (position == 0U ? mock.command : 0U));

	const uint8_t received = (cfg->operation & SPI_MODE_LOOP) != 0U ? transmitted : 
		                 (position == 0U ? (uint8_t)(mock.command ^ 0xffU) : 
				 reply_byte(mock.command, position - 1U));

	mock.tx_wire[position] = transmitted;
	mock.rx_wire[position] = received;
	clock_rx(&rx_cursor, received);

	++mock.frames;
    }

    return mock.result;
}


/* Fail-fast API checks must leave caller storage and configuration untouched */
static void expect_rejected(const struct spi_client *arg, bool null_reply, size_t length, int error) {
    uint8_t storage[SPI_COMMAND_MAX_REPLY + 8U], before[sizeof(storage)];
    uint8_t config_before[sizeof(config)];

    memset(storage, 0xd3, sizeof(storage));
    memcpy(before, storage, sizeof(storage));
    memcpy(config_before, &config, sizeof(config));

    CHECK(mock.calls == 0U);
    CHECK(spi_command_read(arg, 0x9fU, null_reply ? NULL : storage + 3U, length) == error);
    CHECK(mock.calls == 0U);
    CHECK(memcmp(storage, before, sizeof(storage)) == 0);
    CHECK(memcmp(&config, config_before, sizeof(config)) == 0);
    ++cases;
}

/* Valid reply bytes plus canaries before/after and an untouched trailing suffix */
static void exercise_transfer(uint8_t command, size_t length, size_t wire_limit, int driver_result) {
    uint8_t storage[SPI_COMMAND_MAX_REPLY + 16U];
    uint8_t config_before[sizeof(config)], client_before[sizeof(client)];
    const size_t begin = 4U + (command % 3U);

    memset(storage, 0xcc, sizeof(storage));
    memcpy(config_before, &config, sizeof(config));
    memcpy(client_before, &client, sizeof(client));

    mock.armed = true;
    mock.bus = &bus;
    mock.config = &config;
    mock.command = command;
    mock.reply = storage + begin;
    mock.reply_len = length;
    mock.wire_limit = wire_limit;
    mock.result = driver_result;
    
    CHECK(spi_client_ready(&client) == 0);
    CHECK(mock.calls == 0U);
    CHECK(spi_command_read(&client, command, storage + begin, length) == driver_result);
    CHECK(mock.calls == 1U && mock.frames == wire_limit);
    CHECK(mock.discarded == (wire_limit == 0U ? 0U : 1U));
    CHECK(memcmp(&config, config_before, sizeof(config)) == 0);
    CHECK(memcmp(&client, client_before, sizeof(client)) == 0);

    const size_t delivered = wire_limit == 0U ? 0U : wire_limit - 1U;
     
    CHECK(delivered <= length);

    for (size_t i = 0U; i < sizeof(storage); ++i) {
        uint8_t expected = 0xccU;

	if (i >= begin && i - begin < delivered) {
	    expected = (config.operation & SPI_MODE_LOOP) != 0U ? 0U : reply_byte(command, i - begin);
	}

	CHECK(storage[i] == expected);
    }

    ++cases;
}

static void test_arguments(void) {
    configure(LOOPBACK, 0U);

    CHECK(spi_client_ready(NULL) == -EINVAL);
    expect_rejected(NULL, false, 1U, -EINVAL);

    configure(LOOPBACK, 0U);
    client.bus = NULL;

    CHECK(spi_client_ready(&client) == -EINVAL);
    expect_rejected(&client, false, 1U, -EINVAL);

    configure(LOOPBACK, 0U);
    client.config = NULL;

    CHECK(spi_client_ready(&client) == -EINVAL);
    expect_rejected(&client, false, 1U, -EINVAL);

    configure(LOOPBACK, 0U);
    expect_rejected(&client, true, 1U, -EINVAL);

    configure(LOOPBACK, 0U);
    expect_rejected(&client, false, 0U, -EINVAL);

    configure(LOOPBACK, 0U);
    expect_rejected(&client, true, 0U, -EINVAL);

    const size_t invalid_lengths[] = {17U, 255U, 256U, SIZE_MAX / 2U, SIZE_MAX};

    for (size_t i = 0U; i < sizeof(invalid_lengths)/sizeof(invalid_lengths[0]); ++i) {
        configure(LOOPBACK, 0U);
	expect_rejected(&client, false, invalid_lengths[i], -EMSGSIZE);
    }

    /* Characterize source validation precedence when multiple fields are bad */
    configure(LOOPBACK, 0U);
    expect_rejected(NULL, true, SIZE_MAX, -EINVAL);

    configure(LOOPBACK, 0U);
    expect_rejected(NULL, false, SIZE_MAX, -EMSGSIZE);
}


static void test_readiness(void) {
    const uint32_t frequencies[] = {1U, 781250U, UINT32_MAX};

    for (unsigned int mode = 0U; mode < 4U; ++mode) {
        for (unsigned int loop = 0U; loop < 2U; ++loop) {
	    for (unsigned int high = 0U; high < 2U; ++high) {
	        for (unsigned int low = 0U; low < 2U; ++low) {
		    for (unsigned int port = 0U; port < 3U; ++port) {
		        configure(GPIO_LOW, mode);

			config.operation |= (spi_operation_t)((loop ? SPI_MODE_LOOP : 0U) |
					                     (high ? SPI_CS_ACTIVE_HIGH : 0U));
			config.cs.gpio.dt_flags = low ? GPIO_ACTIVE_LOW : GPIO_ACTIVE_HIGH;
			config.cs.gpio.port = port == 0U ? NULL : &gpio;
			gpio.ready = port == 2U;
			
			const int expected = port == 0U ? -EINVAL : 
				                     (port == 1U ? -ENODEV : (low == high ? -EINVAL : 0));

			CHECK(spi_client_ready(&client) == expected);

			if (expected != 0) {
			    expect_rejected(&client, false, 1U, expected);
			} else {
			    exercise_transfer(0x9U, 3U, 4U, 0);
			}
		    }
		}
	    }
	}
    }

    for (unsigned int mode = 0U; mode < 4U; ++mode) {
        for (unsigned int high = 0U; high < 2U; ++high) {
	    configure(LOOPBACK, mode);
	    config.operation = (spi_operation_t)((config.operation & ~SPI_MODE_LOOP) |
			                         (high ? SPI_CS_ACTIVE_HIGH : 0U));

	    CHECK(spi_client_ready(&client) == -ENOTSUP);
	    expect_rejected(&client, false, 1U, -ENOTSUP);

	    configure(LOOPBACK, mode);
	    config.operation |= (spi_operation_t)(high ? SPI_CS_ACTIVE_HIGH : 0U);
	    exercise_transfer(0xa5U, 1U, 2U, 0);
	}
    }

    for (unsigned int profile = 0U; profile < PROFILE_COUNT; ++profile) {
        configure((enum profile) profile, 0U);
	bus.ready = false;

	CHECK(spi_client_ready(&client) == -ENODEV);
	expect_rejected(&client, false, 1U, -ENODEV);

	configure((enum profile) profile, 0U);
	config.frequency = 0U;

	CHECK(spi_client_ready(&client) == -EINVAL);
	expect_rejected(&client, false, 1U, -EINVAL);

	for (size_t f = 0U; f < sizeof(frequencies)/sizeof(frequencies[0]); ++f) {
            configure((enum profile) profile, 0U);
	    config.frequency = frequencies[f];

	    exercise_transfer(0x9fU, 3U, 4U, 0);
	}
    }
}

static void test_operation_flags(void) {
    for (unsigned int width = 0U; width < 64U; ++width) {
        configure(LOOPBACK, 0U);
	config.operation = (spi_operation_t)(SPI_MODE_LOOP | SPI_WORD_SET(width));

	if (width == 8U) {
	    exercise_transfer(0x31U, 2U, 3U, 0);
	} else {
	    CHECK(spi_client_ready(&client) == -ENOTSUP);
	    expect_rejected(&client, false, 1U, -ENOTSUP);
	}
    }

    const uint32_t unsupported[] = {
        SPI_OP_MODE_PERIPHERAL, SPI_TRANSFER_LSB, SPI_HALF_DUPLEX,
	SPI_FRAME_FORMAT_TI, SPI_HOLD_ON_CS, SPI_LOCK_ON,

#ifdef CONFIG_SPI_EXTENDED_MODES
        SPI_LINES_DUAL, SPI_LINES_QUAD, SPI_LINES_OCTAL,
	UINT32_C(1) << 18, UINT32_C(1) << 31,
#endif
    };

    for (unsigned int profile = 0U; profile < PROFILE_COUNT; ++profile) {
        for (unsigned int mode = 0U; mode < 4U; ++mode) {
            for (size_t i = 0U; i < sizeof(unsupported)/ sizeof(unsupported[0]); ++i) {
	        configure((enum profile) profile, mode);
		config.operation |= (spi_operation_t) unsupported[i];

		CHECK(spi_client_ready(&client) == -ENOTSUP);
		expect_rejected(&client, false, 1U, -ENOTSUP);
	    }
	}
    }
}

static void test_transfer_matrix(void) {
    for (unsigned int profile = 0U; profile < PROFILE_COUNT; ++profile) {
        for (unsigned int mode = 0U; mode < 4U; ++mode) {
            for (unsigned int command = 0U; command <= UINT8_MAX; ++command) {
                for (size_t len = 1U; len <= SPI_COMMAND_MAX_REPLY; ++len) {
		    configure((enum profile) profile, mode);

		    exercise_transfer((uint8_t) command, len, 1U + len, 0);
		}
	    }
	}
    }
}

static void test_driver_errors(void) {
    const int errors[] = {-EIO, -ETIMEDOUT, -ENODEV, -EBUSY, -EINVAL, -ENOTSUP};

    for (unsigned int profile = 0U; profile < PROFILE_COUNT; ++profile) {
        for (size_t e = 0U; e < sizeof(errors)/sizeof(errors[0]); ++e) {
	    for (size_t len = 1U; len <= SPI_COMMAND_MAX_REPLY; ++len) {
	        /* Before command, command only, each partial reply, full reply */
		for (size_t wire = 0U; wire <= 1U + len; ++wire) {
		    configure((enum profile) profile, (unsigned int)(len % 4U));

		    exercise_transfer(0x9fU, len, wire, errors[e]);
		}
	    }
	}
    }
}

static void test_independent_clients(void) {
    struct device other_bus = {true};

    for (unsigned int round = 0U; round < 64U; ++round) {
        configure(GPIO_LOW, round % 4U);

	struct spi_config second;
	memcpy(&second, &config, sizeof(second));
	second.frequency = 1000000U;
	second.peripheral = 42U;

        struct spi_client second_client = {&other_bus, &second};
	uint8_t output[3] = {0xccU, 0xccU, 0xccU};

	mock.armed = true;
	mock.bus = &other_bus;
	mock.config = &second;
	mock.command = 0x9fU;
	mock.reply = output;
	mock.reply_len = 3U;
	mock.wire_limit = 4U;

	CHECK(spi_client_ready(&second_client) == 0 && mock.calls == 0U);
	CHECK(spi_command_read(&second_client, 0x9fU, output, sizeof(output)) == 0);
	CHECK(mock.calls == 1U);
	CHECK(output[0] == 0xefU && output[1] == 0x40U && output[2] == 0x18U);

	++cases;

	/* a previous driver failure must not introduce hidden sticky state */
	configure(GPIO_LOW, 0U);
	exercise_transfer(0x00U, 2U, 2U, -EIO);

	memset(&mock, 0, sizeof(mock));
	exercise_transfer(0xffU, 2U, 3U, 0);
    }
}

static void run_group(const char *name, void (*test)(void)){
    const uint64_t before = cases;
    group = name;
    test();
    printf("PASS [%s]: %" PRIu64 " cases\n", name, cases - before);
}

int main(void) {
    run_group("argument rejection", test_arguments);
    run_group("readiness and CS policy", test_readiness);
    run_group("word sizes and operation flags", test_operation_flags);
    run_group("commands, lengths, modes and buffer bounds", test_transfer_matrix);
    run_group("driver errors and partial replies", test_driver_errors);
    run_group("independent clients and later calls", test_independent_clients);

    printf("PASS: %" PRIu64 " cases; %" PRIu64 " assertions; spi_operation_t=%zu bits\n", cases, checks, sizeof(spi_operation_t) * (size_t)(CHAR_BIT));
    puts("Host SPI API contract only; not a Zephyr, PL022, sensor, or physical CS timing test");
    return 0;
}

