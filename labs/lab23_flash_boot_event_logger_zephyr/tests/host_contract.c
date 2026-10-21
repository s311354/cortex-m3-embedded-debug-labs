#include "boot_log.h"

#include <zephyr/drivers/flash.h>

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

#define MAX_REGION_SIZE 0x1000U
#define GUARD_SIZE      32U
#define GUARD_VALUE     0xa5U
#define DEFAULT_BASE    ((off_t)64)
#define DEFAULT_SLOTS   6U
#define DEFAULT_REGION  (DEFAULT_SLOTS * BOOT_LOG_RECORD_SIZE)

_Static_assert(BOOT_LOG_RECORD_SIZE == 16U, "the on-flash slot contract is 16 bytes");

static unsigned long checks;
static unsigned long cases;
static const char *test_name = "setup";

#define CHECK(expr) do { \
	++checks; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL [%s] %s:%d %s\n", test_name, __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device bus = {
    .ready = true
};

/* Const fixtures also preserve flash_parameters.write_block_size's API type */
#define GEOMETRIES(value) \
    { .write_block_size = 0U, .erase_value = (value) }, \
    { .write_block_size = 1U, .erase_value = (value) }, \
    { .write_block_size = 2U, .erase_value = (value) }, \
    { .write_block_size = 3U, .erase_value = (value) }, \
    { .write_block_size = 4U, .erase_value = (value) }, \
    { .write_block_size = 8U, .erase_value = (value) }, \
    { .write_block_size = 16U, .erase_value = (value) }, \
    { .write_block_size = 32U, .erase_value = (value) }

static const struct flash_parameters geometries[] = {
    GEOMETRIES(0xffU),
    GEOMETRIES(0x00U),
};

static const uint8_t erase_values[] = { 0xffU, 0x00U };
static const int driver_errors[] = {-EIO, -ETIMEDOUT };

static struct {
    const struct flash_parameters *params;
    bool parameters_available;
    off_t base;
    size_t region_size;
    uint8_t bytes[GUARD_SIZE + MAX_REGION_SIZE + GUARD_SIZE];
    unsigned int reads, writes, block_queries, parameter_queries;
    unsigned int fail_read_call;
    int read_error, write_error;
    size_t program_bytes, corrupt_byte;
} mock;

static bool all_bytes(const uint8_t *data, size_t size, uint8_t value) {
    for (size_t i = 0U; i < size; ++i) {
        if (data[i] != value)
	    return false;
    }

    return true;
}

static void clear_faults(void) {
    mock.fail_read_call = 0U;
    mock.read_error = -EIO;
    mock.write_error = 0;
    mock.program_bytes = BOOT_LOG_RECORD_SIZE;
    mock.corrupt_byte = BOOT_LOG_RECORD_SIZE;
}

static void reset_flash(off_t base, size_t region_size, size_t block, uint8_t erase) {
    CHECK(base >= 0);
    CHECK(region_size <= MAX_REGION_SIZE);

    memset(&mock, 0, sizeof(mock));

    for (size_t i = 0U; i < ARRAY_SIZE(geometries); ++i) {
        if (geometries[i].write_block_size == block && geometries[i].erase_value == erase) {
	    mock.params = &geometries[i];
	    break;
	}
    }
    CHECK(mock.params != NULL);

    mock.parameters_available = true;
    mock.base = base;
    mock.region_size = region_size;
    memset(mock.bytes, GUARD_VALUE, sizeof(mock.bytes));
    memset(mock.bytes + GUARD_SIZE, erase, region_size);
    clear_faults();

    bus.ready = true;

    ++cases;
}

static void reset_default(uint8_t erase) {
    reset_flash(DEFAULT_BASE, DEFAULT_REGION, 4U, erase);
}

static uint8_t *slot(size_t index) {
    CHECK(index < mock.region_size / BOOT_LOG_RECORD_SIZE);

    return mock.bytes + GUARD_SIZE + index * BOOT_LOG_RECORD_SIZE;
}

static void check_guards(void) {
    CHECK(all_bytes(mock.bytes, GUARD_SIZE, GUARD_VALUE));

    const size_t end = GUARD_SIZE + mock.region_size;

    CHECK(all_bytes(mock.bytes + end, sizeof(mock.bytes) - end, GUARD_VALUE));
}

/* Map logical flash offsets to a small guarded partition */
static size_t checked_offset(const struct device *dev, off_t offset, size_t size) {
    CHECK(dev == &bus);
    CHECK(bus.ready);
    CHECK(offset >= mock.base);

    const uintmax_t relative = (uintmax_t)offset - (uintmax_t)mock.base;

    CHECK(relative <= mock.region_size);
    CHECK(size <= mock.region_size - (size_t)relative);

    return GUARD_SIZE + (size_t) relative;
}

size_t flash_get_write_block_size(const struct device *dev) {
    CHECK(dev == &bus && bus.ready);

    ++mock.block_queries;

    return mock.params->write_block_size;
}

const struct flash_parameters *flash_get_parameters(const struct device *dev) {
    CHECK(dev == &bus && bus.ready);

    ++mock.parameter_queries;

    return mock.parameters_available ? mock.params : NULL;
}

int flash_read(const struct device *dev, off_t offset, void *data, size_t size) {
    CHECK(data != NULL);
    CHECK(size == BOOT_LOG_RECORD_SIZE);

    const size_t start = checked_offset(dev, offset, size);
    ++mock.reads;

    if (mock.fail_read_call == mock.reads) {
        return mock.read_error;
    }

    memcpy(data, mock.bytes + start, size);
    return 0;
}

int flash_write(const struct device *dev, off_t offset, const void *data, size_t size) {
    CHECK(data != NULL);
    CHECK(size == BOOT_LOG_RECORD_SIZE);
    CHECK(mock.params->write_block_size != 0U);
    CHECK((uintmax_t)offset % mock.params->write_block_size == 0U);
    CHECK(size % mock.params->write_block_size == 0U);
    
    const size_t start = checked_offset(dev, offset, size);

    /* The logger must never program an occupied slot, even with identical bytes */
    CHECK(all_bytes(mock.bytes + start, size, mock.params->erase_value));
    CHECK(mock.program_bytes <= size);

    ++mock.writes;
    memcpy(mock.bytes + start, data, mock.program_bytes);

    if (mock.corrupt_byte < size) {
        mock.bytes[start + mock.corrupt_byte] = mock.params->erase_value;
    }

    return mock.write_error;
}

int flash_erase(const struct device *dev, off_t offset, size_t size) {
    (void) dev;
    (void) offset;
    (void) size;

    /* Erasing could destroy earlier boot events or an adjacent partition */
    CHECK(false);
    return -EPERM;
}

static void make_record(uint8_t out[BOOT_LOG_RECORD_SIZE], unsigned int sequence) {
    for (size_t i = 0U; i < BOOT_LOG_RECORD_SIZE; ++i) {
        out[i] = (uint8_t) (sequence + 17U * (unsigned int)i);

	if (out[i] == mock.params->erase_value)
            out[i] ^= 0x80U;
    }
}

static void seed_slot(size_t index, unsigned int sequence) {
    uint8_t record[BOOT_LOG_RECORD_SIZE];

    make_record(record, sequence);
    memcpy(slot(index), record, sizeof(record));
}

static void open_log(struct boot_log *log, size_t expected_next) {
    CHECK(boot_log_init(log, &bus, mock.base, mock.region_size) == 0);
    CHECK(log->initialized);
    CHECK(log->dev == &bus);
    CHECK(log->base == mock.base && log->region_size == mock.region_size);
    CHECK(log->erase_value == mock.params->erase_value);
    CHECK(log->next_slot == expected_next);
}

static void check_unusable(struct boot_log *log) {
    uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];

    make_record(record, 51U);
    const unsigned int reads = mock.reads, writes = mock.writes;

    CHECK(!log->initialized);
    CHECK(boot_log_append(log, record) == -EINVAL);
    CHECK(boot_log_read_last(log, out) == -EINVAL);
    CHECK(mock.reads == reads && mock.writes == writes);
}

static uintmax_t offset_max(void) {
    return (UINTMAX_C(1) << (sizeof(off_t) * CHAR_BIT - 1U)) - 1U;
}

static void test_argument(void) {
    reset_default(0xffU);
    struct boot_log log = {0};
    uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];

    make_record(record, 1U);
    CHECK(boot_log_init(NULL, &bus, mock.base, mock.region_size) == -EINVAL);
    CHECK(boot_log_append(NULL, record) == -EINVAL);
    CHECK(boot_log_read_last(NULL, out) == -EINVAL);

    check_unusable(&log);

    CHECK(mock.reads == 0U && mock.writes == 0U);

    const struct {
        const struct device *dev;
	off_t base;
	size_t size;
    } invalid[] = {
        {NULL, DEFAULT_BASE, DEFAULT_REGION},
	{&bus, (off_t)-1, DEFAULT_REGION},
	{&bus, DEFAULT_BASE, 0U},
	{&bus, DEFAULT_BASE, BOOT_LOG_RECORD_SIZE - 1U},
	{&bus, DEFAULT_BASE, BOOT_LOG_RECORD_SIZE + 1U},
	{&bus, (off_t)(offset_max() - BOOT_LOG_RECORD_SIZE + 1U), BOOT_LOG_RECORD_SIZE},
    };

    for (size_t i = 0U; i < ARRAY_SIZE(invalid); ++i) {
        reset_default(0xffU);
	open_log(&log, 0U);
	const unsigned int reads = mock.reads;

	CHECK(boot_log_init(&log, invalid[i].dev, invalid[i].base, invalid[i].size) == -EINVAL);
	check_unusable(&log);
	CHECK(mock.reads == reads && mock.writes == 0U);
	check_guards();
    }

    if ((uintmax_t)SIZE_MAX > offset_max()) {
        reset_default(0xffU);
	open_log(&log, 0U);
	const unsigned int reads = mock.reads;

	CHECK(boot_log_init(&log, &bus, 0, (size_t)(offset_max() + 1U)) == -EINVAL);
	check_unusable(&log);
	CHECK(mock.reads == reads && mock.writes == 0U);
    }

    reset_default(0xffU);
    open_log(&log, 0U);
    const unsigned int reads = mock.reads;

    CHECK(boot_log_append(&log, NULL) == -EINVAL);
    CHECK(boot_log_read_last(&log, NULL) == -EINVAL);
    CHECK(log.initialized && log.next_slot == 0U);
    CHECK(mock.reads == reads && mock.writes == 0U);

    /* A context without a device is not usable even if its flag is set */
    log.dev = NULL;

    CHECK(boot_log_append(&log, record) == -EINVAL);
    CHECK(boot_log_read_last(&log, out) == -EINVAL);
    CHECK(mock.reads == reads && mock.writes == 0U);
}

static void test_geometry(void) {
    const size_t invalid_blocks[] = { 0U, 3U, 32U };
    const size_t aligned_blocks[] = { 2U, 4U, 8U, 16U };
    struct boot_log log = {0};

    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
	for (size_t i = 0U; i < ARRAY_SIZE(invalid_blocks); ++i) {
            reset_flash(DEFAULT_BASE, DEFAULT_REGION, invalid_blocks[i], erase_values[e]);
    
    	    CHECK(boot_log_init(&log, &bus, mock.base, mock.region_size) == -ENOTSUP);
    	    check_unusable(&log);
    	    CHECK(mock.reads == 0U && mock.writes == 0U);
        }
    
        for (size_t i = 0U; i < ARRAY_SIZE(aligned_blocks); ++i) {
            reset_flash(DEFAULT_BASE + 1, DEFAULT_REGION, aligned_blocks[i], erase_values[e]);
    
    	    CHECK(boot_log_init(&log, &bus, mock.base, mock.region_size) == -ENOTSUP);
    	    check_unusable(&log);
    	    CHECK(mock.reads == 0U && mock.writes == 0U);
        }
    
        reset_default(erase_values[e]);
        bus.ready = false;
    
        CHECK(boot_log_init(&log, &bus, mock.base, mock.region_size) == -ENODEV);
        check_unusable(&log);
        CHECK(mock.block_queries == 0U && mock.parameter_queries == 0U);
        CHECK(mock.reads == 0U && mock.writes == 0U);
    
        reset_default(erase_values[e]);
        mock.parameters_available = false;
        
        CHECK(boot_log_init(&log, &bus, mock.base, mock.region_size) == -ENOTSUP);
        check_unusable(&log);
        CHECK(mock.reads == 0U && mock.writes == 0U);
    
        /* Base needs write-block alignment */
        reset_flash(3, BOOT_LOG_RECORD_SIZE, 1U, erase_values[e]);
        open_log(&log, 0U);
	uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
	make_record(record, 7U);

	CHECK(boot_log_append(&log, record) == 0);
	CHECK(boot_log_read_last(&log, out) == 0);
	CHECK(memcmp(out, record, sizeof(out)) == 0);
	check_guards();

	/* Exercise the accepted endpoint without allocating a huge flash buffer */
	reset_flash((off_t)(offset_max() - BOOT_LOG_RECORD_SIZE), BOOT_LOG_RECORD_SIZE, 1U, erase_values[e]);
	open_log(&log, 0U);
	make_record(record, 9U);

	CHECK(boot_log_append(&log, record) == 0);
	CHECK(boot_log_read_last(&log, out) == 0);
	CHECK(memcmp(out, record, sizeof(out)) == 0);
	check_guards();
    }
}

static void test_occupancy_patterns(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (unsigned int pattern = 0U; pattern < (1U << DEFAULT_SLOTS); ++pattern) {
            reset_default(erase_values[e]);
	    size_t occupied = 0U;

	    for (size_t i = 0U; i < DEFAULT_SLOTS; ++i) {
	        if ((pattern & (1U << i)) != 0U) {
		    seed_slot(i, (unsigned int)i + 1U);
		    ++occupied;
		}
	    }

	    uint8_t before[sizeof(mock.bytes)], out[BOOT_LOG_RECORD_SIZE];
	    memcpy(before, mock.bytes, sizeof(before));
	    struct boot_log log = {0};

	    /* Independent oracle */
	    const bool valid = (pattern & (pattern + 1U)) == 0U;
	    const int rc = boot_log_init(&log, &bus, mock.base, mock.region_size);
	    CHECK(rc == (valid ? 0 : -EILSEQ));

	    if (valid) {
	        CHECK(log.initialized && log.next_slot == occupied);
		CHECK(mock.reads == DEFAULT_SLOTS);
		CHECK(boot_log_read_last(&log, out) == (occupied == 0U ? -ENOENT : 0));

		if (occupied != 0U) {
		    CHECK(memcmp(out, slot(occupied - 1U), sizeof(out)) == 0);
		}
	    } else {
	        check_unusable(&log);
	    }

	    CHECK(mock.writes == 0U);
	    CHECK(memcmp(mock.bytes, before, sizeof(before)) == 0);
	    check_guards();
	}
    }
}

static void test_single_programmed_byte(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t byte = 0U; byte < BOOT_LOG_RECORD_SIZE; ++byte) {
            reset_default(erase_values[e]);
	    slot(0U)[byte] = (uint8_t) ~erase_values[e];
	    struct boot_log log = {0};
	    open_log(&log, 1U);

	    uint8_t out[BOOT_LOG_RECORD_SIZE];

	    CHECK(boot_log_read_last(&log, out) == 0);
	    CHECK(memcmp(out, slot(0U), sizeof(out)) == 0);
	    CHECK(mock.writes == 0U);
	    check_guards();
	}
    }
}

static void test_append_and_reopen(void) {
    const size_t blocks[] = { 1U, 2U, 4U, 8U, 16U };
    const struct { 
        off_t base; 
	size_t size; 
    } layouts[] = {
	{ 0, BOOT_LOG_RECORD_SIZE },
	{ DEFAULT_BASE, DEFAULT_REGION },
	{ (off_t)0x7000, 0x1000U },
    };

    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t b = 0U; b < ARRAY_SIZE(blocks); ++b) {
            for (size_t layout = 0U; layout < ARRAY_SIZE(layouts); ++layout) {
	        reset_flash(layouts[layout].base, layouts[layout].size, blocks[b], erase_values[e]);
		struct boot_log log = {0};
		open_log(&log, 0U);

		uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
		uint8_t expected[sizeof(mock.bytes)];

		memcpy(expected, mock.bytes, sizeof(expected));
		const unsigned int empty_reads = mock.reads;

		CHECK(boot_log_read_last(&log, out) == -ENOENT);
		CHECK(mock.reads == empty_reads);

		const size_t capacity = mock.region_size / BOOT_LOG_RECORD_SIZE;

		for (size_t i = 0U; i < capacity; ++i) {
                    make_record(record, (unsigned int) i + 1U);
		    const unsigned int reads = mock.reads, writes = mock.writes;

		    CHECK(boot_log_append(&log, record) == 0);
		    CHECK(log.initialized && log.next_slot == i + 1U);
		    CHECK(mock.writes == writes + 1U && mock.reads == reads + 2U);

		    memcpy(expected + GUARD_SIZE + i * BOOT_LOG_RECORD_SIZE, record, sizeof(record));

		    CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);
		    CHECK(boot_log_read_last(&log, out) == 0);
		    CHECK(memcmp(out, record, sizeof(out)) == 0);
		}

		const unsigned int reads = mock.reads, writes = mock.writes;

		CHECK(boot_log_append(&log, record) == -ENOSPC);
		CHECK(log.initialized && log.next_slot == capacity);
		CHECK(mock.reads == reads && mock.writes == writes);
		CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);

		struct boot_log recovered = {0};
		open_log(&recovered, capacity);

		CHECK(boot_log_read_last(&recovered, out) == 0);
		CHECK(memcmp(out, record, sizeof(out)) == 0);
		CHECK(boot_log_append(&recovered, record) == -ENOSPC);
		CHECK(mock.writes == writes);
		check_guards();
	    }
	}
    }
}

static void test_prewrite_failures(void) {
    for (size_t e = 0; e < ARRAY_SIZE(erase_values); ++e) {
        reset_default(erase_values[e]);
	struct boot_log log = {0};
	open_log(&log, 0U);

	uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
	uint8_t before[sizeof(mock.bytes)];

	memset(record, erase_values[e], sizeof(record));
	memcpy(before, mock.bytes, sizeof(before));
	unsigned int reads = mock.reads;

	CHECK(boot_log_append(&log, record) == -EINVAL);
	CHECK(log.initialized && log.next_slot == 0U);
	CHECK(mock.reads == reads && mock.writes == 0U);
	CHECK(memcmp(mock.bytes, before, sizeof(before)) == 0);

	/* All-zero is a valid payload for 0xff erase, and vice versa */
	memset(record, (uint8_t)~erase_values[e], sizeof(record));

	CHECK(boot_log_append(&log, record) == 0);
	CHECK(boot_log_read_last(&log, out) == 0);
	CHECK(memcmp(out, record, sizeof(out)) == 0);

	reads = mock.reads;
	const unsigned int writes = mock.writes;
	bus.ready = false;

	CHECK(boot_log_append(&log, record) == -ENODEV);
	CHECK(boot_log_read_last(&log, out) == -ENODEV);
	CHECK(log.initialized && log.next_slot == 1U);
	CHECK(mock.reads == reads && mock.writes == writes);

	bus.ready = true;

	CHECK(boot_log_append(&log, record) == 0);
	check_guards();

	for (size_t error = 0; error < ARRAY_SIZE(driver_errors); ++error) {
            reset_default(erase_values[e]);
	    seed_slot(0U, 1U);
	    open_log(&log, 1U);
	    make_record(record, 2U);
	    memcpy(before, mock.bytes, sizeof(before));
	    mock.fail_read_call = mock.reads + 1U;
	    mock.read_error = driver_errors[error];
	    
	    CHECK(boot_log_append(&log, record) == driver_errors[error]);
	    CHECK(log.initialized && log.next_slot == 1U);
	    CHECK(mock.writes == 0U);
	    CHECK(memcmp(mock.bytes, before, sizeof(before)) == 0);

	    clear_faults();

	    CHECK(boot_log_append(&log, record) == 0);
	    CHECK(log.next_slot == 2U);
	    check_guards();
	}

	for (size_t byte = 0U; byte < BOOT_LOG_RECORD_SIZE; ++byte) {
           reset_default(erase_values[e]);

	   open_log(&log, 0U);
	   slot(0U)[byte] = (uint8_t) ~erase_values[e];
           memcpy(before, mock.bytes, sizeof(before));
	   make_record(record, 3U);

	   CHECK(boot_log_append(&log, record) == -EEXIST);
	   CHECK(log.next_slot == 0U && mock.writes == 0U);
	   check_unusable(&log);

	   open_log(&log, 1U);

	   CHECK(boot_log_append(&log, record) == 0);
	   CHECK(memcmp(slot(0U), before + GUARD_SIZE, BOOT_LOG_RECORD_SIZE) == 0);
	   check_guards();
	}
    }
}

static void test_scan_read_errors(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t at = 0U; at < DEFAULT_SLOTS; ++at) {
	    for (size_t error = 0U; error < ARRAY_SIZE(driver_errors); ++error) {
	        reset_default(erase_values[e]);
		seed_slot(0U, 1U);
		seed_slot(1U, 2U);

		uint8_t before[sizeof(mock.bytes)];
		memcpy(before, mock.bytes, sizeof(before));
		struct boot_log log = { .initialized = true };
		mock.fail_read_call = (unsigned int) at + 1U;
		mock.read_error = driver_errors[error];

		CHECK(boot_log_init(&log, &bus, mock.base, mock.region_size) == driver_errors[error]);
		CHECK(mock.reads == at + 1U && mock.writes == 0U);
		check_unusable(&log);
		CHECK(memcmp(mock.bytes, before, sizeof(before)) == 0);

		clear_faults();
		open_log(&log, 2U);

		check_guards();
	    }
	}
    }
}

static void test_torn_write_recovery(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t progress = 0U; progress <= BOOT_LOG_RECORD_SIZE; ++progress) {
            for (size_t error = 0U; error < ARRAY_SIZE(driver_errors); ++error) {
	        reset_default(erase_values[e]);
		seed_slot(0U, 1U);
		struct boot_log log = {0};
		open_log(&log, 1U);
		uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
		uint8_t expected[sizeof(mock.bytes)];
		make_record(record, 2U);
		memcpy(expected, mock.bytes, sizeof(expected));
		memcpy(expected + GUARD_SIZE + BOOT_LOG_RECORD_SIZE, record, progress);
		mock.program_bytes = progress;
		mock.write_error = driver_errors[error];
		const unsigned int reads = mock.reads;

		CHECK(boot_log_append(&log, record) == driver_errors[error]);
		CHECK(log.next_slot == 1U);
		CHECK(mock.reads == reads + 1U && mock.writes == 1U);
		CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);
		check_unusable(&log);

		clear_faults();
		struct boot_log recovered = {0};
		const size_t next = progress == 0U ? 1U : 2U;
		open_log(&recovered, next);

		CHECK(boot_log_read_last(&recovered, out) == 0);
		CHECK(memcmp(out, slot(next - 1U), sizeof(out)) == 0);

		/* A nonempty torn slot is occupied */
		if (progress > 0U && progress < BOOT_LOG_RECORD_SIZE) {
		    CHECK(memcmp(out, record, sizeof(out)) != 0);
		}

		make_record(record, 3U);
		CHECK(boot_log_append(&recovered, record) == 0);
		memcpy(expected + GUARD_SIZE + next * BOOT_LOG_RECORD_SIZE, record, sizeof(record));
		CHECK(recovered.next_slot == next + 1U);
		CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);
		check_guards();
	    }
	}
    }
}


static void test_readback_failures(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t error = 0; error < ARRAY_SIZE(driver_errors); ++error) {
            reset_default(erase_values[e]);
	    seed_slot(0U, 1U);
	    struct boot_log log = {0};
	    open_log(&log, 1U);

	    uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
	    make_record(record, 2U);
	    mock.fail_read_call = mock.reads + 2U;
	    mock.read_error = driver_errors[error];

	    CHECK(boot_log_append(&log, record) == driver_errors[error]);
	    CHECK(log.next_slot == 1U && mock.writes == 1U);
	    CHECK(memcmp(slot(1U), record, sizeof(record)) == 0);
	    check_unusable(&log);

	    clear_faults();
	    open_log(&log, 2U);

	    CHECK(boot_log_read_last(&log, out) == 0);
	    CHECK(memcmp(out, record, sizeof(out)) == 0);
	    check_guards();
	}

	for (size_t byte = 0U; byte < BOOT_LOG_RECORD_SIZE; ++byte) {
            reset_default(erase_values[e]);
	    seed_slot(0U, 1U);
	    struct boot_log log = {0};
	    open_log(&log, 1U);

	    uint8_t record[BOOT_LOG_RECORD_SIZE], out[BOOT_LOG_RECORD_SIZE];
	    uint8_t expected[sizeof(mock.bytes)];
	    make_record(record, 2U);
	    memcpy(expected, mock.bytes, sizeof(expected));
	    memcpy(expected + GUARD_SIZE + BOOT_LOG_RECORD_SIZE, record, sizeof(record));
	    expected[GUARD_SIZE + BOOT_LOG_RECORD_SIZE + byte] = erase_values[e];
	    mock.corrupt_byte = byte;

	    CHECK(boot_log_append(&log, record) == -EIO);
	    CHECK(log.next_slot == 1U && mock.writes == 1U);
	    CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);
	    check_unusable(&log);

	    clear_faults();
	    open_log(&log, 2U);

	    CHECK(boot_log_read_last(&log, out) == 0);
	    CHECK(memcmp(out, slot(1U), sizeof(out)) == 0);
	    CHECK(memcmp(out, record, sizeof(out)) != 0);

	    make_record(record, 3U);

	    CHECK(boot_log_append(&log, record) == 0);
	    memcpy(expected + GUARD_SIZE + 2U * BOOT_LOG_RECORD_SIZE, record, sizeof(record));
	    CHECK(memcmp(mock.bytes, expected, sizeof(expected)) == 0);
	    check_guards();
	}
    }
}

static void test_read_last_failures(void) {
    for (size_t e = 0U; e < ARRAY_SIZE(erase_values); ++e) {
        for (size_t error = 0U; error < ARRAY_SIZE(driver_errors); ++error) {
            reset_default(erase_values[e]);
	    seed_slot(0U, 1U);
	    seed_slot(1U, 2U);
	    struct boot_log log = {0};
	    open_log(&log, 2U);

	    uint8_t out[BOOT_LOG_RECORD_SIZE], before[sizeof(mock.bytes)];
	    memcpy(before, mock.bytes, sizeof(before));
	    mock.fail_read_call = mock.reads + 1U;
	    mock.read_error = driver_errors[error];

	    CHECK(boot_log_read_last(&log, out) == driver_errors[error]);
	    CHECK(log.initialized && log.next_slot == 2U);
	    CHECK(mock.writes == 0U);

	    clear_faults();

	    CHECK(boot_log_read_last(&log, out) == 0);
	    CHECK(memcmp(out, slot(1U), sizeof(out))  == 0);

	    /* Simulate external loss of the last slot */
	    memset(slot(1U), erase_values[e], BOOT_LOG_RECORD_SIZE);

	    CHECK(boot_log_read_last(&log, out) == -EIO);
	    CHECK(log.initialized && log.next_slot == 2U);
	    CHECK(mock.writes == 0U);
	    check_guards();
	}
    }  
}


int main(void) {
    const struct {
	const char *name;
        void (*run)(void);
    } tests[] = {
        { "argument validation", test_argument },
        { "device and flash geometry", test_geometry },
	{ "exhaustive slot occupancy", test_occupancy_patterns },
	{ "single-byte occupied slots", test_single_programmed_byte },
	{ "append, capacity, and reopne", test_append_and_reopen },
	{ "pre-write failure and retry", test_prewrite_failures },
	{ "scan read-error propagation", test_scan_read_errors },
	{ "torn-write recovery", test_torn_write_recovery },
        { "readback error and corruption", test_readback_failures },
	{ "read-last faulures", test_read_last_failures },
    };

    for (size_t i = 0U; i < ARRAY_SIZE(tests); ++i) {
        test_name = tests[i].name;
	tests[i].run();
    }

    printf("PASS: %lu cases; %lu assertions\n", cases, checks);
    puts("Host flash API/recovery contract only; not a Zephyr firmware or physical flash power-loss test");
    return EXIT_SUCCESS;
}


