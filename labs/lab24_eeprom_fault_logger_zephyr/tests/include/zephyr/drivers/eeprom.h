/* Host-only declarations matching the public APIs consumed by fault_log.c */
#ifndef LAB24_HOST_EEPROM_H
#define LAB24_HOST_EEPROM_H

#include <stddef.h>
#include <sys/types.h>

#include <zephyr/device.h>

int eeprom_read(const struct device *dev, off_t offset, void *data, size_t len);
int eeprom_write(const struct device *dev, off_t offset, const void *data, size_t len);

size_t eeprom_get_size(const struct device *dev);

#endif
