/* Host-only EEPROM model  */

#include "fault_log.h"

#include <zephyr/drivers/eeprom.h>

#include <errno.h>
#include <limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(FAULT_LOG_SLOT_COUNT == 8U && FAULT_LOG_SLOT_SIZE == 32U &&
	       FAULT_LOG_BYTES == 256U && FAULT_LOG_BODY_SIZE == 16U &&
	       FAULT_LOG_COMMIT_OFFSET == 24U && FAULT_LOG_COMMIT_SIZE == 8U,
	       "Update the wire-format fixtures when the logger format changes");

enum {
    MODEL_BYTES = 512,
    PAGE_BYTES = 8
};

static const char *test_group = "startup";
static size_t assertions, cases;

#define CHECK(expr) do { \
	++assertions; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL [%s] %s:%d %s\n", test_group, __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device dev = {
    .ready = true,
    .size = MODEL_BYTES
};
static uint8_t memory[MODEL_BYTES];

static unsigned int reads, writes, byte_count, page_count;
static int fail_read, fail_write, silent_write, partial, corrupt_read;
static int read_error = -ETIMEDOUT, write_error = -EIO;
static int cut_byte = -1, cut_page = -1, cut_read = -1;

struct io_event {
    char kind;
    off_t offset;
    size_t len;
};
static struct io_event trace[32];
static size_t trace_count;
static bool tracing;

static const struct fault_log *observed_ctx;
static uint32_t observed_sequence;
static uint8_t observed_slot;

static void record_io(char kind, off_t offset, size_t len) {
    if (tracing) {
        CHECK(trace_count < sizeof(trace)/ sizeof(trace[0]));
	trace[trace_count++] = (struct io_event) {kind, offset, len};
    }

    if (observed_ctx != NULL) {
        CHECK(!observed_ctx->initialized);
	CHECK(observed_ctx->sequence == observed_sequence);
	CHECK(observed_ctx->newest_slot == observed_slot);
    }
}

static void check_outside_region(void) {
    for (size_t i = FAULT_LOG_BYTES; i < sizeof(memory); ++i) {
        CHECK(memory[i] == 0x6dU);
    }
}

static unsigned int tear_mask;
static uint32_t random_page;
static jmp_buf power_cut;
static const uint8_t token[8] = {0xa5, 0x5a, 0xc3, 0x3c, 0x96, 0x69, 0xf0, 0x0f};

static uint32_t rng(uint32_t *s) {
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

static void clean_controls(void) {
    reads = writes = byte_count = page_count = 0U;
    fail_read = fail_write = silent_write = 0;
    partial = corrupt_read = 0;

    read_error = -ETIMEDOUT;
    write_error = -EIO;

    cut_byte = cut_page = cut_read = -1;

    trace_count = 0U;
    tracing = false;

    observed_ctx = NULL;

    tear_mask = 0U;
    random_page = 0U;
}

static void blank(uint8_t value){
    memset(memory, value, FAULT_LOG_BYTES);
    memset(memory + FAULT_LOG_BYTES, 0x6d, sizeof(memory) - FAULT_LOG_BYTES);

    dev.ready = true;
    dev.size = sizeof(memory);
    clean_controls();
}

int eeprom_read(const struct device *d, off_t offset, void *out, size_t len) {
    CHECK(d == &dev && out != NULL && offset >= 0);
    CHECK((size_t)offset <= d->size && len <= d->size - (size_t)offset);
    CHECK((size_t)offset <= FAULT_LOG_BYTES && len <= FAULT_LOG_BYTES - (size_t)offset);

    ++reads;
    record_io('R', offset, len);

    if ((int) reads == fail_read)
	return read_error;

    memcpy(out, memory + offset, len);

    if ((int) reads == corrupt_read && len != 0U)
	((uint8_t *)out)[0] ^= 1U;

    if ((int) reads == cut_read)
        longjmp(power_cut, 1);

    return 0;
}

size_t eeprom_get_size(const struct device *d) {
    CHECK(d == &dev);

    return d->size;
}

int eeprom_write(const struct device *d, off_t offset, const void *input, size_t len) {
    CHECK(d == &dev && input != NULL && offset >= 0);
    CHECK((size_t) offset <= d->size && len <= d->size - (size_t)offset);
    CHECK((size_t) offset <= FAULT_LOG_BYTES && len <= FAULT_LOG_BYTES - (size_t) offset);

    const uint8_t *p = input;
    ++writes;
    record_io('W', offset, len);

    /* Each append is invalidate(8), body(16), publish(8) */
    const unsigned int phase = (writes - 1U) % 3U;

    CHECK((phase == 1U && len == 16U && (offset % 32) == 0) ||
          (phase != 1U && len == 8U && (offset % 32) == 24));

    if ((int) writes == silent_write)
        return 0;

    if ((int) writes == fail_write) {
        size_t count = (size_t) partial;

	if (count > len) 
            count = len;

	memcpy(memory + offset, p, count);
	return write_error;
    }

    if (cut_byte == 0) {
        longjmp(power_cut, 1);
    }

    for (size_t start = 0U; start < len; start += 8U) {
        ++page_count;

	if ((int) page_count == cut_page) {
	    for (size_t k = 0U; k < 8U; ++k) 
		if (random_page != 0U) 
	            memory[offset + (off_t) start + (off_t)k] = (uint8_t) rng(&random_page);
	        else if ((tear_mask & (1U << k)) != 0U)
	            memory[offset + (off_t) start + (off_t)k] = p[start + k];

	    longjmp(power_cut, 1);
	}

	for (size_t k = 0U; k < 8U; ++k) {
	    memory[offset + (off_t)start + (off_t)k] = p[start + k];
	    ++byte_count;
	    if ((int) byte_count == cut_byte) { longjmp(power_cut, 1); }
	}
    }

    return 0;
}

static void put32(uint8_t *p, uint32_t n) {
    for (unsigned int i = 0; i < 4; ++i)
	p[i] = (uint8_t) (n >> (8U * i));
}

static uint32_t reverse32(uint32_t n) {
    uint32_t r = 0;

    for (unsigned int i = 0; i < 32; ++i) {
        r = (r << 1) | (n & 1U);
        n >>= 1;
    }

    return r;
}	

static uint32_t oracle_crc(const uint8_t *p, size_t n) {
    uint32_t c = UINT32_MAX;

    for (size_t i = 0; i < n; ++i) {
        c ^= reverse32(p[i]);
	for (unsigned int b = 0; b < 8; ++b) {
	    c = (c & UINT32_C(0x80000000)) ? (c << 1) ^ UINT32_C(0x04c11db7) : c << 1;
	}
    }
    return reverse32(c) ^ UINT32_MAX;
}

static void body_for(uint8_t body[16], uint32_t seq, uint32_t code) {
    memcpy(body, "FLT1", 4);

    put32(body + 4, seq);
    put32(body + 8, code);
    put32(body + 12, oracle_crc(body, 12));
}

static void fixture(unsigned int slot, uint32_t seq, uint32_t code) {
    body_for(memory + slot * 32U, seq, code);
    memcpy(memory + slot * 32U + 24U, token, sizeof(token));
}

static struct fault_log populate(unsigned int count, uint8_t erased) {
    blank(erased);
    struct fault_log log;

    CHECK(fault_log_init(&log, &dev) == 0);

    for (unsigned int i = 0; i < count; ++i) {
        CHECK(fault_log_append(&log, UINT32_C(0xabc00000) + i) == 0);
    }

    clean_controls();
    return log;
}

static void basic(void) {
    CHECK(oracle_crc((const uint8_t *)"123456789", 9) == UINT32_C(0xcbf43926));

    for (unsigned int e = 0; e < 2; ++e) {
        struct fault_log log = populate(0, e ? 0xff : 0x00);
	uint32_t out = UINT32_C(0xeeeeeeee);

	CHECK(log.initialized && log.newest_slot == FAULT_LOG_NO_SLOT);
	CHECK(fault_log_latest(&log, &out) == -ENOENT);
	CHECK(out == UINT32_C(0xeeeeeeee) && writes == 0);

	for (unsigned int i = 0; i < 4096; ++i) {
	    const uint32_t code = i == 0 ? 0U : (i == 1 ? UINT32_MAX : i * UINT32_C(2654435761));

	    CHECK(fault_log_append(&log, code) == 0);
	    CHECK(log.sequence == i + 1U && log.newest_slot == i % 8U);
	    CHECK(fault_log_latest(&log, &out) == 0 && out == code);
	    
	    struct fault_log rebooted;

            CHECK(fault_log_init(&rebooted, &dev) == 0);
	    CHECK(rebooted.sequence == log.sequence && rebooted.newest_slot == log.newest_slot);

	    log = rebooted;
	    ++cases;
	}
    }

    struct fault_log log = populate(3, 0xff);
    const unsigned int before = writes;

    CHECK(fault_log_init(NULL, &dev) == -EINVAL);
    CHECK(fault_log_init(&log, NULL) == -EINVAL && !log.initialized);

    dev.ready = false;
    CHECK(fault_log_init(&log, &dev) == -ENODEV);

    dev.ready = true;
    dev.size = 255;
    CHECK(fault_log_init(&log, &dev) == -ENOSPC);

    dev.size = MODEL_BYTES;
    CHECK(fault_log_init(&log, &dev) == 0);
    CHECK(fault_log_append(NULL, 0) == -EINVAL);
    CHECK(fault_log_latest(&log, NULL) == -EINVAL);

    dev.ready = false;
    CHECK(fault_log_append(&log, 0) == -ENODEV);

    dev.ready = true;
    log.initialized = false;
    CHECK(fault_log_append(&log, 0) == -EIO);
    CHECK(writes == before);

    ++cases;
}

/* No variable modified between setjmp and longjmp is relied on afterwards */
static void run_power_cut(const struct fault_log *previous, const uint8_t before[FAULT_LOG_BYTES], uint32_t new_code) {
    uint8_t intended[16];

    const bool was_empty = previous->newest_slot == FAULT_LOG_NO_SLOT;
    const unsigned int target = was_empty ? 0U : (previous->newest_slot + 1U) % 8U;
    const uint32_t seq = was_empty ? 1U : previous->sequence + 1U;

    body_for(intended, seq, new_code);
    memcpy(memory, before, FAULT_LOG_BYTES);

    if (setjmp(power_cut) == 0) {
        struct fault_log interrupted = *previous;

	(void) fault_log_append(&interrupted, new_code);
	CHECK(false);
    }

    clean_controls();

    /* No other slot, including the previous newest, newest, may be disturbed */
    for (unsigned int slot = 0U; slot < FAULT_LOG_SLOT_COUNT; ++slot) {
        if (slot != target)
            CHECK(memcmp(memory + slot * 32U, before + slot * 32U, 32U) == 0);
    }

    check_outside_region();
    const bool new_complete = memcmp(memory + target * 32U, intended, 16) == 0 && 
	                      memcmp(memory + target * 32U + 24U, token, 8) == 0;

    struct fault_log recovered;

    CHECK(fault_log_init(&recovered, &dev) == 0);

    uint32_t result = UINT32_C(0xeeeeeeee);
    
    int rc = fault_log_latest(&recovered, &result);

    if (new_complete) {
        CHECK(rc == 0 && result == new_code && recovered.sequence == seq);
    } else if (was_empty) {
	CHECK(rc == -ENOENT && result == UINT32_C(0xeeeeeeee));
    } else {
        CHECK(rc == 0 && recovered.newest_slot == previous->newest_slot && 
	      recovered.sequence == previous->sequence);

	const uint8_t *p = before + previous->newest_slot * 32U + 8U;

	const uint32_t old_code = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
		                  ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);

	CHECK(result == old_code);
    }

    CHECK(writes == 0);
    ++cases;
}

static void cuts_for(const struct fault_log *prior) {
    uint8_t before[FAULT_LOG_BYTES];

    memcpy(before, memory, sizeof(before));

    for (int boundary = 0; boundary <= 32; ++boundary) {
        clean_controls();

	cut_byte = boundary;
	run_power_cut(prior, before, UINT32_C(0xdeadbeef));
    }

    for (int page = 1; page <= 4; ++page) {
        for (unsigned int mask = 0U; mask < 256U; ++mask) {
            clean_controls();

	    cut_page = page;
	    tear_mask = mask;
	    run_power_cut(prior, before, UINT32_C(0xdeadbeef));
	}

	for (uint32_t seed = 1U; seed <= 128U; ++seed) {
	    clean_controls();

	    cut_page = page;
	    random_page = seed;
	    run_power_cut(prior, before, UINT32_C(0xdeadbeef));
	}
    }

    for (int read = 1; read <= 3; ++read) {
        clean_controls();
	cut_read = read;
	run_power_cut(prior, before, UINT32_C(0xdeadbeef));
    }
}

static void power_tests(void) {
    const unsigned int counts[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16, 17};

    for (unsigned int erased = 0; erased < 2; ++erased) {
        for (size_t n = 0; n < sizeof(counts)/sizeof(counts[0]) ; ++n) {
	    const struct fault_log prior = populate(counts[n], erased ? 0xff : 0x00);
	    cuts_for(&prior);
	}

	blank(erased ? 0xff : 0x00);

	for (unsigned int slot = 0U; slot < FAULT_LOG_SLOT_COUNT; ++slot) {
	    fixture(slot, UINT32_MAX - 7U + slot, UINT32_C(0x90000000) + slot);
	}

	struct fault_log prior;

	CHECK(fault_log_init(&prior, &dev) == 0);
	CHECK(prior.sequence == UINT32_MAX && prior.newest_slot == 7U);
	cuts_for(&prior);
    }
}

static void error_tests(void) {
    for (int op = 1; op <= 8; ++op) {
        struct fault_log log = populate(8, 0xff);

	clean_controls();
	fail_read = op;

	CHECK(fault_log_init(&log, &dev) == -ETIMEDOUT);
	CHECK(!log.initialized && writes == 0);
	++cases;
    }

    for (int op = 1; op <= 3; ++op) {
        for (int n = 0; n <= (op == 2 ? 16 : 8) ; ++n) {
            struct fault_log log = populate(8, 0xff);

	    fail_write = op;
	    partial = n;

	    CHECK(fault_log_append(&log, 12345) == -EIO);
	    CHECK(!log.initialized);

	    const unsigned int count = writes;

	    CHECK(fault_log_append(&log, 12345) == -EIO && writes == count);

	    clean_controls();

	    CHECK(fault_log_init(&log, &dev) == 0);

	    uint32_t code;
	    CHECK(fault_log_latest(&log, &code) == 0);
	    CHECK(code == (op == 3 && n == 8 ? 12345U : UINT32_C(0xabc00007)));

	    ++cases;
	}

	struct fault_log log = populate(8, 0xff);
	fail_read = op;

	CHECK(fault_log_append(&log, 12345) == -ETIMEDOUT);
	CHECK(!log.initialized);

	clean_controls();

	CHECK(fault_log_init(&log, &dev) == 0);

	uint32_t code;
	CHECK(fault_log_latest(&log, &code) == 0);
	CHECK(code == (op == 3 ? 12345U : UINT32_C(0xabc00007)));

	++cases;

	log = populate(8, 0xff);
	silent_write = op;

	CHECK(fault_log_append(&log, 12345) == -EIO && !log.initialized);
	++cases;
    } 

    for (unsigned int byte = 0; byte < 32; ++byte) {
        for (unsigned int bit = 0; bit < 8; ++bit) {
	    struct fault_log log = populate(8, 0xff);
	    memory[7U * 32U + byte] ^= (uint32_t)(1U << bit);

	    uint32_t code = 77;
	    const bool spare = byte >= 16 && byte < 24;

	    CHECK(fault_log_latest(&log, &code) == (spare ? 0 : -EILSEQ));

	    if (!spare) {
	        CHECK(code == 77 && !log.initialized);
	    }

	    CHECK(fault_log_init(&log, &dev) == 0);
	    CHECK(log.newest_slot == (spare ? 7 : 6));
	    ++cases;
	}
    }

    clean_controls();

    struct fault_log log = populate(8, 0xff);
    fail_read = 1;
    uint32_t code = 77;

    CHECK(fault_log_latest(&log, &code) == -ETIMEDOUT && code == 77);
    ++cases;
}


static void history_tests(void) {
    blank(0xff);

    fixture(0, UINT32_C(0xfffffffd), 0);
    fixture(1, UINT32_C(0xfffffffe), 1);
    fixture(2, UINT32_MAX, 2);

    struct fault_log log;

    CHECK(fault_log_init(&log, &dev) == 0 && log.sequence == UINT32_MAX);

    for (unsigned int i = 0; i < 32; ++i) {
        CHECK(fault_log_append(&log, 99 + i) == 0);
	CHECK(log.sequence == i);
	CHECK(fault_log_init(&log, &dev) == 0 && log.sequence == i);
	++cases;
    }

    blank(0xff);
    fixture(0, 12, 1);
    fixture(7, 12, 2);

    CHECK(fault_log_init(&log, &dev) == -EILSEQ && !log.initialized);
    ++cases;

    blank(0xff);
    fixture(0, 0, 1);
    fixture(7, UINT32_C(0x80000000), 2);

    CHECK(fault_log_init(&log, &dev) == -EILSEQ);
    ++cases;

    blank(0xff);
    fixture(0, 100, 1);
    fixture(5, 101, 2);

    CHECK(fault_log_init(&log, &dev) == 0 && log.newest_slot == 5);
    ++cases;

    blank(0xff);
    fixture(0, 100, 1);
    memory[0] = 'X';
    put32(memory + 12, oracle_crc(memory, 12));

    CHECK(fault_log_init(&log, &dev) == 0 && log.newest_slot == FAULT_LOG_NO_SLOT);
    ++cases;
}

static void validation_and_protocol(void) {
    const size_t sizes[] = {0U, 1U, 255U, 256U, 512U};

    for (size_t i = 0U; i < sizeof(sizes)/sizeof(sizes[0]); ++i) {
        blank(0xff);

	dev.size = sizes[i];
	struct fault_log log = {
	    .dev = &dev,
	    .sequence = 99U,
	    .newest_slot = 3U,
	    .initialized = true
	};

	const bool enough = sizes[i] >= FAULT_LOG_BYTES;

	CHECK(fault_log_init(&log, &dev) == (enough ? 0 : -ENOSPC));
	CHECK(log.initialized == enough);
	CHECK(log.newest_slot == FAULT_LOG_NO_SLOT && log.sequence == 0U);
	CHECK(reads == (enough ? 8U : 0U) && writes == 0U);

	if (!enough) {
	    CHECK(log.dev == NULL);
	}

	check_outside_region();
	++cases;
    }

    struct fault_log log = populate(2U, 0xff);
    uint32_t out = UINT32_C(0xdecafbad);

    CHECK(fault_log_latest(NULL, &out) == -EINVAL);
    CHECK(fault_log_latest(&log, NULL) == -EINVAL);
    CHECK(out == UINT32_C(0xdecafbad));

    dev.ready = false;
    CHECK(fault_log_latest(&log, &out) == -ENODEV);
    CHECK(out == UINT32_C(0xdecafbad) && reads == 0U && writes == 0U);

    dev.ready = true;
    log.initialized = false;
    CHECK(fault_log_latest(&log, &out) == -EIO);

    log.initialized = true;
    log.newest_slot = FAULT_LOG_SLOT_COUNT;

    CHECK(fault_log_latest(&log, &out) == -EINVAL);
    CHECK(fault_log_append(&log, 4U) == -EINVAL);
    CHECK(out == UINT32_C(0xdecafbad) && reads == 0U && writes == 0U);

    ++cases;

    /* Reuse each target */
    for (unsigned int count = 0U; count <= 16U; ++count) {
        log = populate(count, 0xff);

	const unsigned int target = count % FAULT_LOG_SLOT_COUNT;
	const off_t base = (off_t)(target * FAULT_LOG_SLOT_SIZE);
	const struct io_event expected [] = {
	    {'W', base + 24, 8U}, {'R', base + 24, 8U}, 
	    {'W', base, 16U},    {'R', base, 16U},
	    {'W', base + 24, 8U}, {'R', base, 32U},
	};

	uint8_t before[FAULT_LOG_BYTES];
	memcpy(before, memory, sizeof(before));
	observed_ctx = &log;
	observed_sequence = log.sequence;
	observed_slot = log.newest_slot;
        tracing = true;

	CHECK(fault_log_append(&log, UINT32_C(0x01020304)) == 0);

	observed_ctx = NULL;
	tracing = false;

	CHECK(log.initialized && log.sequence == count + 1U);
	CHECK(log.newest_slot == target && writes == 3U && reads == 3U);
	CHECK(trace_count == sizeof(expected)/sizeof(expected[0]));

	for (size_t i = 0U; i < trace_count; ++i) {
	    CHECK(trace[i].kind == expected[i].kind);
	    CHECK(trace[i].offset == expected[i].offset);
	    CHECK(trace[i].len == expected[i].len);
	}

	uint8_t intended[16];
	body_for(intended, count + 1U, UINT32_C(0x01020304));

	CHECK(memcmp(memory + base, intended, sizeof(intended)) == 0);
	CHECK(memcmp(memory + base + 24, token, sizeof(token)) == 0);

	/* Reserved page in the selected slot is not written either */
	CHECK(memcmp(memory + base + 16, before + base + 16, 8U) == 0);

	for (unsigned int slot = 0U; slot < FAULT_LOG_SLOT_COUNT; ++slot) {
	    if (slot != target) {
	        CHECK(memcmp(memory + slot * 32U, before + slot * 32U, 32U) == 0);
	    }
	}
	check_outside_region();
	++cases;
    }
}

static void additional_errors(void) {
    const int errors[] = {-EIO, -ETIMEDOUT, -ENODEV};

    for (size_t e = 0U; e < sizeof(errors)/sizeof(errors[0]); ++e) {
        for (int call = 1; call <= 3; ++call) {
            struct fault_log log = populate(8U, 0xff);

	    fail_write = call;
	    write_error = errors[e];
	    partial = 0;

	    CHECK(fault_log_append(&log, 12345U) == errors[e]);
	    CHECK(!log.initialized && log.sequence == 8U && log.newest_slot == 7U);
	    CHECK(writes == (unsigned int)call && reads == (unsigned int)(call - 1));

	    uint32_t out = 77U;

	    CHECK(fault_log_latest(&log, &out) == -EIO && out == 77U);
	    CHECK(fault_log_append(&log, 12345U) == -EIO);
	    CHECK(writes == (unsigned int) call);
	    check_outside_region();
	    ++cases;

	    log = populate(8U, 0xff);
	    fail_read = call;
	    read_error = errors[e];

	    CHECK(fault_log_append(&log, 12345U) == errors[e]);
	    CHECK(!log.initialized && log.sequence == 8U && log.newest_slot == 7U);
	    CHECK(writes == (unsigned int) call && reads == (unsigned int) call);

	    clean_controls();

	    CHECK(fault_log_init(&log, &dev) == 0);
	    CHECK(fault_log_latest(&log, &out) == 0);
	    CHECK(out == (call == 3 ? 12345U : UINT32_C(0xabc00007)));

	    check_outside_region();
	    ++cases;
	}
    }

    for (int call = 1; call <= 3; ++call) {
        struct fault_log log = populate(8U, 0xff);
	corrupt_read = call;

	CHECK(fault_log_append(&log, 9876U) == -EIO);
	CHECK(!log.initialized && writes == (unsigned int) call);

	clean_controls();

	CHECK(fault_log_init(&log, &dev) == 0);

	uint32_t out = 77U;

	CHECK(fault_log_latest(&log, &out) == 0);
	CHECK(out == (call == 3 ? 9876U : UINT32_C(0xabc00007)));

	++cases;
    }

    /* Good CRC but a sequence not matching cached metadata must be rejected */
    struct fault_log log = populate(8U, 0xff);
    fixture(7U, 9U, 999U);
    uint32_t out = 77U;

    CHECK(fault_log_latest(&log, &out) == -EILSEQ);
    CHECK(out == 77U && !log.initialized && writes == 0U);
    ++cases;
}

static void run_group(const char *name, void (*fn)(void)) {
    const size_t start = cases;
    test_group = name;
    fn();
    printf("PASS [%s]: %zu cases\n", name, cases - start);
}

int main(void) {
    run_group("basic append/latest/recovery", basic);
    run_group("validation and publish ordering", validation_and_protocol);
    run_group("power-cut recovery", power_tests);
    run_group("I/O errors and stored corruption", error_tests);
    run_group("error propagation and RAM invalidation", additional_errors);
    run_group("sequence rollover and invalid histories", history_tests);
    printf("PASS: %zu cases; %zu assertions\n", cases, assertions);
    puts("Host EEPROM contract only; not a Zephyr firmware, QEMU, or physical power-loss test");
    return 0;
}

