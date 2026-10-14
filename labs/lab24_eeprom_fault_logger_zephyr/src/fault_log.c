#include "fault_log.h"

#include <errno.h>
#include <stddef.h>
#include <string.h>

#include <zephyr/drivers/eeprom.h>

/* Body pages and commit page are disjoint even during page-local tearing */
static const uint8_t magic[4] = {'F', 'L', 'T', '1'};
static const uint8_t committed[FAULT_LOG_COMMIT_SIZE] = {
    0xa5U, 0x5aU, 0xc3U, 0x3cU, 0x96U, 0x69U, 0xf0U, 0x0fU
};

static const uint8_t invalid[FAULT_LOG_COMMIT_SIZE] = {0};

static uint32_t load_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
	   ((uint32_t)p[2] << 16 | ((uint32_t)p[3]) << 24);
}

static void store_le32(uint8_t *p, uint32_t v) {
    for (unsigned int i = 0U; i < 4U; ++i) {
        p[i] = (uint8_t) (v >> (8U * i));
    }
}

/* CRC-32/ISO-HDLC */
static uint32_t crc32(const uint8_t *p, size_t size) {
    uint32_t crc = UINT32_MAX;

    for (size_t i = 0U; i < size; ++i) {
       crc ^= p[i];

       for (unsigned int bit = 0U; bit < 8U; ++bit) {
           crc = (crc >> 1) ^ ((crc & 1U) ? UINT32_C(0xedb88320) : 0U);
       }
    }

    return crc ^ UINT32_MAX;
}

static off_t slot_offset(uint8_t slot) {
    return (off_t)((unsigned int)slot * FAULT_LOG_SLOT_SIZE);
}

static bool record_valid(const uint8_t data[FAULT_LOG_SLOT_SIZE]) {
    return memcmp(data + FAULT_LOG_COMMIT_OFFSET, committed, sizeof(committed)) == 0 &&
	   memcmp(data, magic, sizeof(magic)) == 0 &&
	   load_le32(data + 12U) == crc32(data, 12U);
}

static bool newer(uint32_t a, uint32_t b) {
    const uint32_t delta = a - b;
    return delta != 0U && delta < UINT32_C(0x80000000);
}

static int usable(const struct fault_log *log) {
    if (log == NULL || log->dev == NULL) {
        return -EINVAL;
    }

    if (!log->initialized) {
        return -EIO;
    }

    if (log->newest_slot != FAULT_LOG_NO_SLOT && log->newest_slot >= FAULT_LOG_SLOT_COUNT) {
        return -EINVAL;
    }

    return device_is_ready(log->dev) ? 0 : -ENODEV;
}

int fault_log_init(struct fault_log *log, const struct device *dev) {
    if (log == NULL)
        return -EINVAL;

    /* Failed initialization must not leave stale metadata usable */
    *log = (struct fault_log) {
        .newest_slot = FAULT_LOG_NO_SLOT
    };

    if ( dev == NULL)
        return -EINVAL;

    if (!device_is_ready(dev))
	return -ENODEV;

    if (eeprom_get_size(dev) < FAULT_LOG_BYTES) {
        return -ENOSPC;
    }

    struct fault_log recovered = {
        .dev = dev,
	.newest_slot = FAULT_LOG_NO_SLOT,
    };

    
    uint8_t bytes[FAULT_LOG_SLOT_SIZE];
    uint32_t sequences[FAULT_LOG_SLOT_COUNT] = {0};
    uint8_t valid_mask = 0U;

    for (uint8_t slot = 0U; slot < FAULT_LOG_SLOT_COUNT; ++slot) {
        const int rc = eeprom_read(dev, slot_offset(slot), bytes, sizeof(bytes));

	if (rc != 0)
	    return rc;

	if (!record_valid(bytes)) {
	   continue;
	}

	const uint32_t seq = load_le32(bytes + 4U);
	sequences[slot] = seq;
	valid_mask |= (uint8_t)(1U << slot);

	if (recovered.newest_slot == FAULT_LOG_NO_SLOT || newer(seq, recovered.sequence)) {
	    recovered.sequence = seq;
	    recovered.newest_slot = slot;
	}
    }

    /* A correctly generated eight-slot ring spans at most seven increments */
    for (uint8_t slot = 0U; slot < FAULT_LOG_SLOT_COUNT; ++slot) {
        if ((valid_mask & (uint8_t)(1U << slot)) != 0U && slot != recovered.newest_slot) {
	    const uint32_t age = recovered.sequence - sequences[slot];

	    if (age == 0U || age >= FAULT_LOG_SLOT_COUNT) {
	        return -EILSEQ;
	    }
	}
    }

    recovered.initialized = true;
    *log = recovered;
    return 0;
}

int fault_log_append(struct fault_log *log, uint32_t fault_code) {
    int rc = usable(log);

    if (rc != 0)
	return rc;

    const bool empty = log->newest_slot == FAULT_LOG_NO_SLOT;
    const uint8_t target = empty ? 0U : (uint8_t) ((log->newest_slot + 1U) % FAULT_LOG_SLOT_COUNT);

    const uint32_t sequence = empty ? 1U : log->sequence + 1U;
    const off_t base = slot_offset(target);
    uint8_t body[FAULT_LOG_BODY_SIZE];
    uint8_t check[FAULT_LOG_SLOT_SIZE];

    memcpy(body, magic, sizeof(magic));
    store_le32(body + 4U, sequence);
    store_le32(body + 8U, fault_code);
    store_le32(body + 12U, crc32(body, 12U));

    /* Any error after this point requires a rescan, not a blind retry */
    log->initialized = false;

    /*  1. Invalidate and verify the old token BEFORE touching its body */
    rc = eeprom_write(log->dev, base + FAULT_LOG_COMMIT_OFFSET, invalid, sizeof(invalid));

    if (rc != 0)
	return rc;

    rc = eeprom_read(log->dev, base + FAULT_LOG_COMMIT_OFFSET, check, sizeof(invalid));

    if (rc != 0)
	return rc;

    if (memcmp(check, invalid, sizeof(invalid)) != 0) {
        return -EIO;
    }

    /* 2. Program and verify the body while the commit page stays invalid */
    rc = eeprom_write(log->dev, base, body, sizeof(body));

    if (rc != 0)
	return rc;

    rc = eeprom_read(log->dev, base, check, sizeof(body));

    if (rc != 0)
        return rc;

    if (memcmp(check, body, sizeof(body)) != 0)
	return -EIO;

    /* 3. Publish last, then verify the whole record before updating RAM */
    rc = eeprom_write(log->dev, base + FAULT_LOG_COMMIT_OFFSET, committed, sizeof(committed));

    if (rc != 0)
        return rc;

    rc = eeprom_read(log->dev, base, check, sizeof(check));

    if (rc != 0)
	return rc;

    if (!record_valid(check) || memcmp(check, body, sizeof(body)) != 0) {
        return -EIO;
    }

    log->sequence = sequence;
    log->newest_slot = target;
    log->initialized = true;
    return 0;
}

int fault_log_latest(struct fault_log *log, uint32_t *fault_code) {
    if (fault_code == NULL) {
        return -EINVAL;
    }

    int rc = usable(log);

    if (rc != 0)
	return rc;

    if (log->newest_slot == FAULT_LOG_NO_SLOT) {
        return -ENOENT;
    }

    uint8_t bytes[FAULT_LOG_SLOT_SIZE];

    rc = eeprom_read(log->dev, slot_offset(log->newest_slot), bytes, sizeof(bytes));

    if (rc != 0)
	return rc;

    if (!record_valid(bytes) || load_le32(bytes + 4U) != log->sequence) {
        log->initialized = false;
	return -EILSEQ;
    }

    *fault_code = load_le32(bytes + 8U);

    return 0;
}
