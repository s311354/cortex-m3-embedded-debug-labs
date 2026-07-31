#ifndef EEPROM_H
#define EEPROM_H

#include <stddef.h>
#include <stdint.h>

#include "mps2_i2c.h"

/*
 * Generic EEPROM device descriptor.
 *
 * target_addr:
 *     7-bit I2C slave address, without the R/W bit.
 *
 * address_width:
 *     Number of EEPROM internal-address bytes.
 *     1 = devices such as small 24C02-style EEPROMs
 *     2 = larger EEPROMs using 16-bit memory addresses
 */
struct eeprom_device {
  struct mps2_i2c_bus *bus;
  uint8_t target_addr;
  uint8_t address_width;
  size_t page_size;
  uint32_t ready_poll_limit;
};

enum eeprom_status {
  EEPROM_OK                = 0,
  EEPROM_ERR_ARGUMENT      = -100,
  EEPROM_ERR_ADDRESS_WIDTH = -101,
  EEPROM_ERR_PAGE_BOUNDARY = -102,
  EEPROM_ERR_TRANSFER      = -103,
  EEPROM_ERR_NOT_READY     = -104 
};

int eeprom_write(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  const uint8_t *data,
  size_t length
);

int eeprom_write_byte(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t value
);

int eeprom_read(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t *data,
  size_t length
);

int eeprom_read_byte(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t *value
);

int eeprom_wait_ready(const struct eeprom_device *device);

#endif
