#ifndef LAB21_REG_ACCESS_H
#define LAB21_REG_ACCESS_H

#include <stdint.h>

#include <zephyr/device.h>

/*
 * Required minimal data structure
 *
 * bus:
 *     Zephyr I2C controller device
 *
 * addr:
 *     7-bit address of the target device on that bus
 */
struct i2c_client {
    const struct device *bus;
    uint16_t addr;
};

int reg_read_byte(const struct i2c_client *client, uint8_t reg, uint8_t *value);

int reg_write_byte(const struct i2c_client *client, uint8_t reg, uint8_t value);

int reg_update_byte(const struct i2c_client *client, uint8_t reg, uint8_t mask, uint8_t value);

#endif
