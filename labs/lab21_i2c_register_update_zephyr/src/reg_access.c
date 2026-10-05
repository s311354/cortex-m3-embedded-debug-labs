#include "reg_access.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>

static int validate_client (const struct i2c_client *client) {
    if (client == NULL || client->bus == NULL || client->addr > 0x7fU) {
        return -EINVAL;
    }

    if (!device_is_ready(client->bus)) {
        return -ENODEV;
    }

    return 0;
}

int reg_read_byte(const struct i2c_client *client, uint8_t reg, uint8_t *value) {
    int rc;

    if (value == NULL) {
        return -EINVAL;
    }

    rc = validate_client(client);

    if (rc != 0)
	return rc;

    return i2c_write_read(client->bus, client->addr, &reg, sizeof(reg), value, sizeof(*value));
}

int reg_write_byte(const struct i2c_client *client, uint8_t reg, uint8_t value) {
    int rc;

    uint8_t tx[2] = {
        reg,
	value
    };

    rc = validate_client(client);

    if (rc != 0)
	return rc;

    return i2c_write(client->bus, tx, sizeof(tx), client->addr);
}

int reg_update_byte(const struct i2c_client *client, uint8_t reg, uint8_t mask, uint8_t value) {
    uint8_t old_value;
    uint8_t new_value;

    int rc;

    rc = reg_read_byte(client, reg, &old_value);

    if (rc != 0)
	return rc;

    /* Preserve bits outside mask */
    new_value = (uint8_t) (old_value & (uint8_t)~mask) | (value & mask);

    /* Requirement */
    if (new_value == old_value)
	return 0;

    return reg_write_byte(client, reg, new_value);
}
