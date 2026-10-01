#include <stddef.h>
#include <stdint.h>

#include "eeprom.h"

#define EEPROM_MAX_PAYLOAD_SIZE 32U

static int validate_range(const struct eeprom_device *device, uint16_t memory_addr, size_t length) {
    size_t limit;

    if (device->address_width == 1U)
        limit = 256U;
    else if (device->address_width == 2U)
	limit = 65536U;
    else
	return EEPROM_ERR_ADDRESS_WIDTH;

    if ((size_t) memory_addr >= limit || length > limit - (size_t)memory_addr)
	return EEPROM_ERR_ARGUMENT;

    return EEPROM_OK;
}

static size_t build_memory_address(const struct eeprom_device *device, uint16_t memory_addr, uint8_t *buffer) {
    // 1-byte address
    if (device->address_width == 1U) {
	buffer[0] = (uint8_t)memory_addr;
	return 1U;
    }

    // 2-byte address
    buffer[0] = (uint8_t)(memory_addr >> 8U); // High byte
    buffer[1] = (uint8_t)memory_addr; // Low byte
    return 2;
}

int eeprom_write(const struct eeprom_device *device, uint16_t memory_addr, const uint8_t *data, size_t length) {
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

    result = validate_range(device, memory_addr, length);

    if (result != EEPROM_OK)
	return result;

    if (length > device->page_size - ((size_t) memory_addr % device->page_size))
	return EEPROM_ERR_PAGE_BOUNDARY;

    // Build address bytes
    address_length = build_memory_address(device, memory_addr, tx_buffer);

    // Append data after address bytes
    for (size_t index = 0U; index < length; ++index) {
       tx_buffer[address_length + index] = data[index];
    }

    // Build I2C message
    message.buf = tx_buffer;
    message.len = (uint32_t)(address_length + length);
    message.flags = MPS2_I2C_MSG_WRITE | MPS2_I2C_MSG_STOP;

    // Execute I2C transaction
    result = mps2_i2c_transfer(device->bus, &message, 1U, device->target_addr);

    if (result != MPS2_I2C_OK) {
        return EEPROM_ERR_TRANSFER;
    }

    // Wait for EERPOM internal write cycle to complete
    return eeprom_wait_ready(device);

}

int eeprom_write_byte(const struct eeprom_device *device, uint16_t memory_addr, uint8_t value) {
    return eeprom_write(device, memory_addr, &value, 1U);
}

int eeprom_read(const struct eeprom_device *device, uint16_t memory_addr, uint8_t *data, size_t length) {
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

    result = validate_range(device, memory_addr, length);

    if (result != EEPROM_OK)
	return result;

    address_length = build_memory_address(device, memory_addr, address_buffer);

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

     // Message 1: Write memory address
     messages[0].buf = address_buffer;
     messages[0].len = address_length;
     messages[0].flags = MPS2_I2C_MSG_WRITE;

     // Message 2: Read data (with RESTART and STOP)
     messages[1].buf = data;
     messages[1].len = length;
     messages[1].flags = MPS2_I2C_MSG_READ |
	                 MPS2_I2C_MSG_RESTART |
			 MPS2_I2C_MSG_STOP;

     // Execute combined transaction
     result = mps2_i2c_transfer(device->bus, messages, 2U, device->target_addr);

     return (result == MPS2_I2C_OK) ? EEPROM_OK : EEPROM_ERR_TRANSFER;
}

int eeprom_read_byte(const struct eeprom_device *device, uint16_t memory_addr, uint8_t *value
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
	// Probe: Send address and check for ACK
        result = mps2_i2c_probe(device->bus, device->target_addr);
    
    
        if (result == MPS2_I2C_OK) {
	    return EEPROM_OK;
	}

	if (result != MPS2_I2C_ERR_NACK) {
	    return EEPROM_ERR_TRANSFER;
	}

	// NACK = skill busy, keep polling
    }

    return EEPROM_ERR_NOT_READY;
}
