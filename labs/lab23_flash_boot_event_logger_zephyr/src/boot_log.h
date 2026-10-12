#ifndef LAB23_BOOT_LOG_H
#define LAB23_BOOT_LOG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <zephyr/device.h>

#define BOOT_LOG_RECORD_SIZE 16U

struct boot_log {
    const struct device *dev;

    off_t base;
    size_t region_size;
    size_t next_slot;
    uint8_t erase_value;
    bool initialized;
};

int boot_log_init(struct boot_log *log, const struct device *dev, off_t base, size_t region_size);

int boot_log_append(struct boot_log *log, const uint8_t record[BOOT_LOG_RECORD_SIZE]);

int boot_log_read_last(const struct boot_log *log, uint8_t out[BOOT_LOG_RECORD_SIZE]);

#endif
