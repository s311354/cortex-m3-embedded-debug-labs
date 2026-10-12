#include "boot_log.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/drivers/flash.h>

static bool is_erased(const uint8_t *data, uint8_t erase_value) {
    for (size_t i = 0U; i < BOOT_LOG_RECORD_SIZE; ++i) {
        if (data[i] != erase_value) {
	    return false;
	}
    }

    return true;
}

static off_t slot_offset(const struct boot_log *log, size_t index) {
    return log->base + (off_t) (index * BOOT_LOG_RECORD_SIZE);	
}

int boot_log_init(struct boot_log *log, const struct device *dev, off_t base, size_t region_size) {
    if (log == NULL)
	return -EINVAL;

    memset(log, 0, sizeof(*log));

    if (dev == NULL || base < 0 || region_size < BOOT_LOG_RECORD_SIZE || (region_size % BOOT_LOG_RECORD_SIZE) != 0U) {
        return -EINVAL;
    }

    const uint64_t off_max = (sizeof(off_t) <= sizeof(int32_t)) ? (uint64_t) INT32_MAX : (uint64_t) INT64_MAX;

    if ((uint64_t) base > off_max || (uint64_t) region_size > off_max - (uint64_t) base) {
        return -EINVAL;
    }

    if (!device_is_ready(dev))
	return -ENODEV;

    const size_t write_block = flash_get_write_block_size(dev);

    if (write_block == 0U || (BOOT_LOG_RECORD_SIZE % write_block) != 0U || ((uint64_t) base % write_block) != 0U) {
        return -ENOTSUP;
    }

    const struct flash_parameters *params = flash_get_parameters(dev);

    if (params == NULL)
	return -ENOTSUP;

    log->dev = dev;
    log->base = base;
    log->region_size = region_size;
    log->erase_value = params->erase_value;

    const size_t capacity = region_size / BOOT_LOG_RECORD_SIZE;
    bool found_empty = false;
    uint8_t data[BOOT_LOG_RECORD_SIZE];

    for (size_t index = 0U; index < capacity; ++index) {
        const int rc = flash_read(dev, slot_offset(log, index), data, sizeof(data));

	if (rc != 0)
            return rc;

	if (is_erased(data, log->erase_value)) {
	    found_empty = true;
	} else {
	    if (found_empty) {
	        return -EILSEQ;
	    }

	    log->next_slot = index + 1U;
	}
    }

    log->initialized = true;
    return 0;
}

int boot_log_append(struct boot_log *log, const uint8_t record[BOOT_LOG_RECORD_SIZE]) {
    if (log == NULL || record == NULL || !log->initialized || log->dev == NULL) 
	return -EINVAL;

    if (!device_is_ready(log->dev))
	return -ENODEV;

    if (is_erased(record, log->erase_value))
	return -EINVAL;

    if (log->next_slot >= log->region_size / BOOT_LOG_RECORD_SIZE)
	return -ENOSPC;

    const off_t offset = slot_offset(log, log->next_slot);
    uint8_t check[BOOT_LOG_RECORD_SIZE];
    int rc = flash_read(log->dev, offset, check, sizeof(check));

    if (rc != 0)
	return rc;

    if (!is_erased(check, log->erase_value)) {
        log->initialized = false;
	return -EEXIST;
    }

    log->initialized = false;
    rc = flash_write(log->dev, offset, record, BOOT_LOG_RECORD_SIZE);

    if (rc != 0)
	return rc;

    rc = flash_read(log->dev, offset, check, sizeof(check));

    if (rc != 0)
	return rc;

    if (memcmp(check, record, BOOT_LOG_RECORD_SIZE) != 0)
	return -EIO;

    ++log->next_slot;
    log->initialized = true;
    return 0;
}

int boot_log_read_last(const struct boot_log *log, uint8_t out[BOOT_LOG_RECORD_SIZE]) {
    if (log == NULL || out == NULL || !log->initialized || log->dev == NULL)
	return -EINVAL;

    if (!device_is_ready(log->dev))
	return -ENODEV;

    if (log->next_slot == 0U)
	return -ENOENT;

    const int rc = flash_read(log->dev, slot_offset(log, log->next_slot - 1U), out, BOOT_LOG_RECORD_SIZE);

    if (rc != 0)
	return rc;

    return is_erased(out, log->erase_value) ? -EIO : 0;

}
