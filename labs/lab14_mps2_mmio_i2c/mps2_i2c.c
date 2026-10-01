#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "mps2_i2c.h"

#define MPS2_I2C_SDA_MASK ((uint32_t)(SDA))
#define MPS2_I2C_SCL_MASK ((uint32_t)(SCL))
#define MPS2_I2C_LINES (MPS2_I2C_SDA_MASK | MPS2_I2C_SCL_MASK)


_Static_assert(offsetof(MPS2_I2C_TypeDef, CONTROL) == 0, "CONTROL offset");

static int bus_valid(const struct mps2_i2c_bus *bus) {
    return bus != NULL && bus->regs != NULL && bus->delay_cycles != 0U && bus->timeout_cycles != 0U;
}

/**
 * Software delay for I2C bit timing
 * 
 * Creates precise delays required by I2C protocol timing since the
 * MPS2 I2C peripheral has no built-in baud rate generator.
 */
static void mps2_i2c_bit_delay(const struct mps2_i2c_bus *bus) {
  for (volatile uint32_t count = 0U; count < bus->delay_cycles; ++count)
    __NOP();
}

/**
 * Read SDA line state from MPS2 I2C peripheral
 * 
 * Uses CONTROL register to read current SDA pin state.
 */
static int sda_is_high(const struct mps2_i2c_bus *bus) {
  return ((bus->regs->CONTROL & MPS2_I2C_SDA_MASK) != 0U);
}

/**
 * Read SCL line state from MPS2 I2C peripheral
 * 
 * Uses CONTROL register to read current SCL pin state.
 * Important for clock stretching detection.
 */
static int scl_is_high(const struct mps2_i2c_bus *bus) {
  return ((bus->regs->CONTROL & MPS2_I2C_SCL_MASK) != 0U);
}

static void sda_release(struct mps2_i2c_bus *bus) {
  bus->regs->CONTROLS = MPS2_I2C_SDA_MASK;
  mps2_i2c_bit_delay(bus);
}

static void scl_release(struct mps2_i2c_bus *bus) {
  bus->regs->CONTROLS = MPS2_I2C_SCL_MASK;
  mps2_i2c_bit_delay(bus);
}

static int wait_scl_high(struct mps2_i2c_bus *bus) {
  scl_release(bus);

  for (uint32_t timeout = 0U; timeout < bus->timeout_cycles; ++timeout) {
    if (scl_is_high(bus)) {
        mps2_i2c_bit_delay(bus);
	return MPS2_I2C_OK;
    }
  }

  return MPS2_I2C_ERR_TIMEOUT;
}

static void sda_drive_low(struct mps2_i2c_bus *bus) {
  bus->regs->CONTROLC = MPS2_I2C_SDA_MASK;
  mps2_i2c_bit_delay(bus);
}

static void scl_drive_low(struct mps2_i2c_bus *bus) {
  bus->regs->CONTROLC = MPS2_I2C_SCL_MASK;
  mps2_i2c_bit_delay(bus);
}

static void bus_release(struct mps2_i2c_bus *bus) {
  sda_release(bus);
  scl_release(bus);
}

/**
 * Check if I2C bus is idle
 * 
 * Bus is idle when both SDA and SCL are high.
 * Reads line states from MPS2 I2C peripheral.
 */
static int bus_is_idle(const struct mps2_i2c_bus *bus) {
  return (bus->regs->CONTROL & MPS2_I2C_LINES) == MPS2_I2C_LINES;
}

__attribute__((noinline))
static int generate_start(struct mps2_i2c_bus *bus) {
  int result;

  sda_release(bus);

  result = wait_scl_high(bus);
  if (result != MPS2_I2C_OK)
    return result;

  if (!sda_is_high(bus))
    return MPS2_I2C_ERR_BUS_BUSY;

  /*
   * START:
   * SDA high -> low while SCL is high
   */
  sda_drive_low(bus);
  scl_drive_low(bus);

  return MPS2_I2C_OK;
}

__attribute__((noinline))
static int generate_restart(struct mps2_i2c_bus *bus) {
  return generate_start(bus);
}

__attribute__((noinline))
static int generate_stop(struct mps2_i2c_bus *bus) {
  int result;

  scl_drive_low(bus);
  sda_drive_low(bus);
  result = wait_scl_high(bus);

  sda_release(bus);

  if (result != MPS2_I2C_OK)
    return result;
 
  return bus_is_idle(bus) ? MPS2_I2C_OK : MPS2_I2C_ERR_BUS_BUSY;
}

static int write_bit(struct mps2_i2c_bus *bus, uint8_t bit_value) {
  int result;

  if (bit_value != 0U) 
    sda_release(bus);
  else
    sda_drive_low(bus);

  result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;

  scl_drive_low(bus);

  return MPS2_I2C_OK;
}

static int read_bit(struct mps2_i2c_bus *bus, uint8_t *bit_value) {
  if (bit_value == NULL)
    return MPS2_I2C_ERR_ARGUMENT;

  int result;

  sda_release(bus);

  result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;

  *bit_value = (sda_is_high(bus) != 0) ? 1U : 0U;

  scl_drive_low(bus);

  return MPS2_I2C_OK;
}


static int receive_ack(struct mps2_i2c_bus *bus) {
  uint8_t nack;

  int result = read_bit(bus, &nack);

  if (result != MPS2_I2C_OK)
    return result;

  return (nack == 0U) ? MPS2_I2C_OK : MPS2_I2C_ERR_NACK;
}

__attribute__((noinline))
static int write_byte(struct mps2_i2c_bus *bus, uint8_t value) {

  for (uint8_t bit = 0U; bit < 8U; ++bit) {
     int result = write_bit(bus, (uint8_t)((value & 0x80U) != 0U));

    if (result != MPS2_I2C_OK)
      return result;

    value <<= 1U;
  }

  return receive_ack(bus);
}

__attribute__((noinline))
static int read_byte(struct mps2_i2c_bus *bus, uint8_t *value, int send_ack) {
  if (value == NULL)
    return MPS2_I2C_ERR_ARGUMENT;

  uint8_t input_bit;
  uint8_t received = 0U;

  for (uint8_t bit = 0U; bit < 8U; ++bit) {
    int result = read_bit(bus, &input_bit);

    if (result != MPS2_I2C_OK)
      return result;

    received = (uint8_t) ((received << 1U) | input_bit);
  }

  /*
   * ACK:  SDA low
   * NACK: SDA release/high
   */
  int result = write_bit(bus, (send_ack != 0) ? 0U : 1U);

  if (result == MPS2_I2C_OK) {
      *value = received;
  }

  return result;
}

static int send_address(struct mps2_i2c_bus *bus, uint8_t target_addr, int is_read) {
  uint8_t address_byte = (uint8_t)(target_addr << 1U) | ((is_read != 0) ? 1U : 0U);

  return write_byte(bus, address_byte);
}

int mps2_i2c_init(struct mps2_i2c_bus *bus) {
  if (!bus_valid(bus))
      return MPS2_I2C_ERR_ARGUMENT;

  bus_release(bus);
  return bus_is_idle(bus) ? MPS2_I2C_OK : mps2_i2c_recover_bus(bus);
}

__attribute__((noinline))
int mps2_i2c_transfer(struct mps2_i2c_bus *bus, struct mps2_i2c_msg *messages, size_t num_messages, uint8_t target_addr) {
  int result;
  int active = 0;

  const uint8_t allowed = MPS2_I2C_MSG_READ | MPS2_I2C_MSG_STOP | MPS2_I2C_MSG_RESTART;

  if (!bus_valid(bus) || messages == NULL || num_messages == 0U)
    return MPS2_I2C_ERR_ARGUMENT;

  if (target_addr == 0x7FU)
    return MPS2_I2C_ERR_ADDRESS;

  // Process each message in sequence
  for (size_t index = 0U; index < num_messages; ++index) {
    struct mps2_i2c_msg *message = &messages[index];
    
    if ((message->buf == NULL) || (message->len == 0U) || (message->flags & ~allowed) != 0U) {
      return MPS2_I2C_ERR_ARGUMENT;
    }

    if (index > 0U && !(messages[index - 1U].flags & MPS2_I2C_MSG_STOP)
		   && !(message->flags & MPS2_I2C_MSG_RESTART)) {
	return MPS2_I2C_ERR_ARGUMENT;
    }
  }

  for (size_t index = 0U; index < num_messages; ++index) {
    struct mps2_i2c_msg *message = &messages[index];
    // Send address with R/W bit
    int is_read = ((message->flags & MPS2_I2C_MSG_READ) != 0U);

    result = active ? generate_restart(bus) : generate_start(bus);

    if (result != MPS2_I2C_OK) {
      if (active)
        (void) generate_stop(bus);

      return result;
    }

    active = 1;
    result = send_address(bus, target_addr, is_read);

    if (result != MPS2_I2C_OK) {
      goto stop_on_error;
    }


    for (size_t byte = 0U; byte < message->len; ++byte) {
      result = is_read ? read_byte(bus, &message->buf[byte], byte + 1U < message->len)
	               : write_byte(bus, message->buf[byte]);

      if (result != MPS2_I2C_OK) {
        goto stop_on_error;
      }
    }

    if ((message->flags & MPS2_I2C_MSG_STOP)) {
      result = generate_stop(bus);

      if (result != MPS2_I2C_OK)
        return result;
    }
  }

  return MPS2_I2C_OK;

stop_on_error:
  (void) generate_stop(bus);
  return result;
}

int mps2_i2c_probe(struct mps2_i2c_bus* bus, uint8_t target_addr) {
  if (!bus_valid(bus)) {
    return MPS2_I2C_ERR_ARGUMENT;
  }

  int result;
  int stop_result;

  if (target_addr > 0x7FU)
    return MPS2_I2C_ERR_ADDRESS;

  result = generate_start(bus);

  if (result != MPS2_I2C_OK)
    return result;

  result = send_address(bus, target_addr, 0);

  stop_result = generate_stop(bus);

  if (result != MPS2_I2C_OK)
    return result;

  return stop_result;
}

int mps2_i2c_recover_bus(struct mps2_i2c_bus *bus) {
  if (!bus_valid(bus))
    return MPS2_I2C_ERR_ARGUMENT;

  sda_release(bus);

  int result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;

  if (sda_is_high(bus))
    return MPS2_I2C_OK;

  for (uint8_t pulse = 0U; pulse < 9U; ++pulse) {
    scl_drive_low(bus);

    result = wait_scl_high(bus);

    if (result != MPS2_I2C_OK)
      return result;

    if (sda_is_high(bus))
      break;
  }

  return generate_stop(bus);
}
