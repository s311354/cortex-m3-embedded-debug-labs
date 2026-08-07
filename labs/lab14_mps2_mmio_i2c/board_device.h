#ifndef BOARD_DEVICES_H
#define BOARD_DEVICES_H

#include "eeprom.h"
#include "mps2_i2c.h"

/**
 * Configured to use MPS2_SHIELD0_I2C peripheral (0x40029000).
 */
extern struct mps2_i2c_bus g_shield0_i2c_bus;

/**
 * I2C EEPROM connected to Shield 0 I2C bus at address 0x50.
 */
extern const struct eeprom_device g_board_eeprom;

/**
 * Initialize all board devices
 */
int board_devices_init(void);

#endif
