/* Minimal host-only declaractions for the Zephyr flash APIs used by this lab */
#ifndef LAB23_HOST_FLASH_H
#define LAB23_HOST_FLASH_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <zephyr/device.h>

struct flash_parameters {
    const size_t write_block_size;
    uint8_t erase_value;
};

size_t flash_get_write_block_size(const struct device *dev);
const struct flash_parameters *flash_get_parameters(const struct device *dev);
int flash_read(const struct device *dev, off_t offset, void *data, size_t len);
int flash_write(const struct device *dev, off_t offset, const void *data, size_t len);

/* The test implementation rejects every erase call to protect the no-erase contract */
int flash_erase(const struct device *dev, off_t offset, size_t size);

#endif
