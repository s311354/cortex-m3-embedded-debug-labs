#ifndef BOARD_DEVICES_H
#define BOARD_DEVICES_H

#include "eeprom.h"
#include "mps2_i2c.h"

extern struct mps2_i2c_bus g_shield0_i2c_bus;
extern const struct eeprom_device g_board_eeprom;

int board_devices_init(void);

#endif
