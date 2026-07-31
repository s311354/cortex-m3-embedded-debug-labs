#ifndef MPS2_I2C_H
#define MPS2_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "CMSDK_CM3.h"
#include "SMM_MPS2.h"

/*
 *  Message direction and transaction-control flags
 */
#define MPS2_I2C_MSG_WRITE    0x00U
#define MPS2_I2C_MSG_READ     0x01U
#define MPS2_I2C_MSG_STOP     0x02U
#define MPS2_I2C_MSG_RESTART  0x04U

enum mps2_i2c_status {
  MPS2_I2C_OK            = 0,
  MPS2_I2C_ERR_ARGUMENT  = -1,
  MPS2_I2C_ERR_ADDRESS   = -2,
  MPS2_I2C_ERR_NACK      = -3,
  MPS2_I2C_ERR_TIMEOUT   = -4,
  MPS2_I2C_ERR_BUS_BUSY  = -5
};

struct mps2_i2c_bus {
  MPS2_I2C_TypeDef *regs;
  uint32_t delay_cycles; // software timing paramter
  uint32_t timeout_cycles;
  
  /*
   * Used for QEMU without a shield EEPROM
   * 1: simulate SCL high, slave ack, read data = 0xFF
   * 0: MMIO line status 
   */
  uint8_t  simulate_bus;
};

struct mps2_i2c_msg {
  uint8_t *buf;
  size_t len;
  uint8_t flags;
};

int mps2_i2c_init(struct mps2_i2c_bus *bus);

int mps2_i2c_transfer(struct mps2_i2c_bus *bus, uint8_t target_addr, 
  struct mps2_i2c_msg *messages, size_t num_messages);

int mps2_i2c_probe(struct mps2_i2c_bus *bus, uint8_t target_addr);

int mps2_i2c_recover_bus(struct mps2_i2c_bus *bus);

#endif
