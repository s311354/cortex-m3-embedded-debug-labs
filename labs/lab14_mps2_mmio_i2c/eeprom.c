#include <stddef.h>
#include <stdint.h>

#include "eeprom.h"

#define EEPROM_MAX_PAYLOAD_SIZE 32U

static int build_memory_address(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t *buffer,
  size_t *address_length) {
    if ((device == NULL) ||
	(buffer == NULL) ||
	(address_length == NULL)) {
        return EEPROM_ERR_ARGUMENT;
    }

    if (device->address_width == 1U) {
        if (memory_addr > 0x00FFU)
	    return EEPROM_ERR_ARGUMENT;

	buffer[0] = (uint8_t)memory_addr;
	*address_length = 1U;
	return EEPROM_OK;
    }

    if (device->address_width == 2U) {
        buffer[0] = (uint8_t)(memory_addr >> 8U);
	buffer[1] = (uint8_t)memory_addr;

	*address_length = 2U;
	return EEPROM_OK;
    }

    return EEPROM_ERR_ADDRESS_WIDTH;
}

static int write_crosses_page(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  size_t length) {
    size_t page_offset;

    page_offset = (size_t)memory_addr % device->page_size;

    return ((page_offset + length) > device->page_size);
}


int eeprom_write(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  const uint8_t *data,
  size_t length
) {
    uint8_t tx_buffer[2U + EEPROM_MAX_PAYLOAD_SIZE];
    struct mps2_i2c_msg message;
    size_t address_length;
    int result;

    if ((device == NULL) ||
	(device->bus == NULL) ||
        (data == NULL) ||
	(length == 0U) ||
	(device->page_size == 0U) ||
	(device->page_size > EEPROM_ERR_ARGUMENT) ||
	(length > device->page_size)) {
        return EEPROM_ERR_ARGUMENT;
    }

    if (write_crosses_page(device, memory_addr, length) != 0) {
        return EEPROM_ERR_PAGE_BOUNDARY;
    }

    result = build_memory_address(device, memory_addr, tx_buffer, &address_length);
    
    if (result != EEPROM_OK)
        return result;

    for (size_t index = 0U; index < length; ++index) {
       tx_buffer[address_length + index] = data[index];
    }

    message.buf = tx_buffer;
    message.len = (uint32_t)(address_length + length);
    message.flags = MPS2_I2C_MSG_WRITE | MPS2_I2C_MSG_STOP;

    result = mps2_i2c_transfer(device->bus, device->target_addr, &message, 1U);

    if (result != MPS2_I2C_OK) {
        return EEPROM_ERR_TRANSFER;
    }

    if (device->bus->simulate_bus != 0U)
	return EEPROM_OK;

    return eeprom_wait_ready(device);

}

int eeprom_write_byte(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t value
) {
    return eeprom_write(device, memory_addr, &value, 1U);
}

int eeprom_read(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t *data,
  size_t length
) {
    uint8_t address_buffer[2];
    struct mps2_i2c_msg messages[2];
    size_t address_length;
    int result;

    if ((device == NULL) ||
	(device->bus == NULL) ||
	(data == NULL) ||
	(length == 0U)) {
        return EEPROM_ERR_ARGUMENT;
    }

    result = build_memory_address(device, memory_addr, address_buffer, &address_length);

    if (result != EEPROM_OK)
        return result;

    /*
     * EEPROM random read:
     *
     * START
     * address + W
     * internal memory address
     * REPEATED START
     * address + R
     * data
     * NACK
     * STOP
     */
     messages[0].buf = address_buffer;
     messages[0].len = address_length;
     messages[0].flags = MPS2_I2C_MSG_WRITE;

     messages[1].buf = data;
     messages[1].len = length;
     messages[1].flags = MPS2_I2C_MSG_READ |
	                 MPS2_I2C_MSG_RESTART |
			 MPS2_I2C_MSG_STOP;

     result = mps2_i2c_transfer(device->bus, device->target_addr, messages, 2U);

     return (result == MPS2_I2C_OK) ? EEPROM_OK : EEPROM_ERR_TRANSFER;
}

int eeprom_read_byte(
  const struct eeprom_device *device,
  uint16_t memory_addr,
  uint8_t *value
) {
    return eeprom_read(device, memory_addr, value, 1U);
}

int eeprom_wait_ready(const struct eeprom_device *device) {
    int result;

    if ((device == NULL) ||
	(device->bus == NULL) ||
	(device->ready_poll_limit == 0U)) {
        return EEPROM_ERR_ARGUMENT;
    }

    for (uint32_t attempt = 0U; attempt < device->ready_poll_limit; ++attempt) {
        result = mps2_i2c_probe(device->bus, device->target_addr);
    
    
        if (result == MPS2_I2C_OK) {
	    return EEPROM_OK;
	}

	if (result != MPS2_I2C_ERR_NACK) {
	    return EEPROM_ERR_TRANSFER;
	}
    }

    return EEPROM_ERR_NOT_READY;
}
