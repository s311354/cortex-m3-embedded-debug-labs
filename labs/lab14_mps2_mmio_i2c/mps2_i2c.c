#include <stddef.h>
#include <stdint.h>

#include "mps2_i2c.h"

#define MPS2_I2C_SDA_MASK ((uint32_t)(SDA))
#define MPS2_I2C_SCL_MASK ((uint32_t)(SCL))

static void mps2_i2c_bit_delay(const struct mps2_i2c_bus *bus) {
  volatile uint32_t count;

  for (count = 0U; count < bus->delay_cycles; ++count)
    __asm volatile ("nop");
}

static int sda_is_high(const struct mps2_i2c_bus *bus) {
  if (bus->simulate_bus != 0U)
    return 1;

  return ((bus->regs->CONTROL & MPS2_I2C_SDA_MASK) != 0U);
}

static int scl_is_high(const struct mps2_i2c_bus *bus) {
  if (bus->simulate_bus != 0U)
    return 1;

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
    if (scl_is_high(bus) != 0) 
      return MPS2_I2C_OK;
  
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

static int bus_is_idle(const struct mps2_i2c_bus *bus) {
  return (scl_is_high(bus) != 0) && (sda_is_high(bus) != 0);
}


__attribute__((noinline))
static int generate_start(struct mps2_i2c_bus *bus) {
  int result;

  sda_release(bus);

  result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;

  if ((bus->simulate_bus == 0U) && sda_is_high(bus) == 0)
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
  int result;

  sda_release(bus);

  result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;

  /*
   * Repeated START:
   * SDA high -> low while SCL is high
   */
  sda_drive_low(bus);
  scl_drive_low(bus);

  return MPS2_I2C_OK;
}

__attribute__((noinline))
static int generate_stop(struct mps2_i2c_bus *bus) {
  int result;

  sda_drive_low(bus);

  result = wait_scl_high(bus);

  if (result != MPS2_I2C_OK)
    return result;
 
  /*
   * STOP:
   * SDA low -> high while SCL is high
   */
  sda_release(bus);

  return MPS2_I2C_OK;
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
  int result;

  if (bus->simulate_bus != 0U) {
    sda_release(bus);

    result = wait_scl_high(bus);

    if (result != MPS2_I2C_OK)
      return result;

    scl_drive_low(bus);

    return MPS2_I2C_OK;
  }

  result = read_bit(bus, &nack);

  if (result != MPS2_I2C_OK)
    return result;

  return (nack == 0U) ? MPS2_I2C_OK : MPS2_I2C_ERR_NACK;
}

__attribute__((noinline))
static int write_byte(struct mps2_i2c_bus *bus, uint8_t value) {
  int result;

  for (uint8_t bit = 0U; bit < 8U; ++bit) {
    result = write_bit(bus, (uint8_t)((value & 0x80U) != 0U));

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

  int result;

  for (uint8_t bit = 0U; bit < 8U; ++bit) {
    result = read_bit(bus, &input_bit);

    if (result != MPS2_I2C_OK)
      return result;

    received = (uint8_t) ((received << 1U) | input_bit);
    
  }

  /*
   * ACK:  SDA low
   * NACK: SDA release/high
   */
  result = write_bit(bus, (send_ack != 0) ? 0U : 1U);

  if (result != MPS2_I2C_OK)
    return result;

  *value = received;

  return MPS2_I2C_OK;
}



























static int send_address(struct mps2_i2c_bus *bus, uint8_t target_addr, int is_read) {
  if (target_addr > 0x7FU)
    return MPS2_I2C_ERR_ADDRESS;

  uint8_t address_byte;

  address_byte = (uint8_t)(target_addr << 1U) | ((is_read != 0) ? 1U : 0U);

  return write_byte(bus, address_byte);
}

static int write_message(struct mps2_i2c_bus *bus, const struct mps2_i2c_msg *message) {
  int result;

  for (size_t index = 0U; index < message->len; ++index) {
    result = write_byte(bus, message->buf[index]);
    
    if (result != MPS2_I2C_OK)
      return result;
  }

  return MPS2_I2C_OK;
}

static int read_message(struct mps2_i2c_bus *bus, struct mps2_i2c_msg *message) {
  int result;
  int send_ack;

  for (size_t index = 0U; index < message->len; ++index) {
    send_ack = ((index + 1U) < message->len);

    result = read_byte(bus, &message->buf[index], send_ack);

    if (result != MPS2_I2C_OK)
      return result;
  }

  return MPS2_I2C_OK;
}

int mps2_i2c_init(struct mps2_i2c_bus *bus) {
  if ((bus == NULL) ||
      (bus->regs == NULL) ||
      (bus->delay_cycles == 0U) ||
      (bus->timeout_cycles == 0U)) {
    return MPS2_I2C_ERR_ARGUMENT;
  }

  bus_release(bus);

  if (bus->simulate_bus != 0)
    return MPS2_I2C_OK;

  if (bus_is_idle(bus) == 0)
    return mps2_i2c_recover_bus(bus);

  return MPS2_I2C_OK;
}

__attribute__((noinline))
int mps2_i2c_transfer(struct mps2_i2c_bus *bus, uint8_t target_addr, 
  struct mps2_i2c_msg *messages, size_t num_messages) {
  if ((bus == NULL) ||
      (bus->regs == NULL) ||
      (messages == NULL) ||
      (num_messages == 0U)) {
    return MPS2_I2C_ERR_ARGUMENT;
  }

  int is_read;
  int result;

  if (target_addr == 0x7FU)
    return MPS2_I2C_ERR_ADDRESS;

  for (size_t index = 0U; index < num_messages; ++index) {
    struct mps2_i2c_msg *message = &messages[index];
    
    if ((messages->buf == NULL) ||
	(messages->len == 0U)) {
      return MPS2_I2C_ERR_ARGUMENT;
    }

    if (index == 0U) {
      result = generate_start(bus);
    } else {
      if ((message->flags & MPS2_I2C_MSG_RESTART) == 0U) {
        (void)generate_stop(bus);
	return MPS2_I2C_ERR_ARGUMENT;
      }
      result = generate_restart(bus);
    }

    if (result != MPS2_I2C_OK) {
      (void)generate_stop(bus);
      return result;
    }

    is_read = ((message->flags & MPS2_I2C_MSG_READ) != 0U);

    result = send_address(bus, target_addr, is_read);

    if (result != MPS2_I2C_OK) {
      (void)generate_stop(bus);
      return result; 
    }

    if (is_read != 0)
      result = read_message(bus, message);
    else
      result = write_message(bus, message);

    if (result != MPS2_I2C_OK) {
      (void) generate_stop(bus);
      return result;
    }

    if ((message->flags & MPS2_I2C_MSG_STOP) != 0U) {
      result = generate_stop(bus);

      if (result != MPS2_I2C_OK)
        return result;
    } else if ((index + 1U) == num_messages) {
        (void) generate_stop(bus);
	return MPS2_I2C_ERR_ARGUMENT;
    }
  }

  return MPS2_I2C_OK;
}

int mps2_i2c_probe(struct mps2_i2c_bus* bus, uint8_t target_addr) {
  if ((bus == NULL) || (bus->regs == NULL)) {
    return MPS2_I2C_ERR_ARGUMENT;
  }

  if ((bus == NULL) || (bus->regs == NULL))
    return MPS2_I2C_ERR_ARGUMENT;

  int result;
  int stop_result;

  if (target_addr > 0x7FU)
    return MPS2_I2C_ERR_ADDRESS;

  result = generate_start(bus);

  if (result == MPS2_I2C_OK)
    result = send_address(bus, target_addr, 0);

  stop_result = generate_stop(bus);

  if (result != MPS2_I2C_OK)
    return result;

  return stop_result;
}


int mps2_i2c_recover_bus(struct mps2_i2c_bus *bus) {
  if ((bus == NULL) || (bus->regs == NULL))
    return MPS2_I2C_ERR_ARGUMENT;

  sda_release(bus);

  int result;

  for (uint8_t pulse = 0U; pulse < 9U; ++pulse) {
    if (sda_is_high(bus) != 0)
      break;

    scl_drive_low(bus);

    result = wait_scl_high(bus);

    if (result != MPS2_I2C_OK)
      return result;
  }

  result = generate_stop(bus);

  if (result != MPS2_I2C_OK)
    return result;

  return (bus_is_idle(bus) != 0) ? MPS2_I2C_OK : MPS2_I2C_ERR_BUS_BUSY;
}
