/* Host-only declarations at the API boundary */
#ifndef LAB21_HOST_I2C_H
#define LAB21_HOST_I2C_H

#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>

int i2c_write(const struct device *dev, const uint8_t *buf, uint32_t num_bytes, uint16_t addr);

int i2c_write_read(const struct device *dev, uint16_t addr, const void *write_buf, size_t num_write, 
		   void *read_buf, size_t num_read);

#endif
