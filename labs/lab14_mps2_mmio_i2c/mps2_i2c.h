#ifndef MPS2_I2C_H
#define MPS2_I2C_H

#include <stddef.h>
#include <stdint.h>

#include "CMSDK_CM3.h"
#include "SMM_MPS2.h"

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

/**
 * @brief MPS2 I2C bus configuration
 * 
 * Represents a software-driven I2C bus using the MPS2 I2C peripheral.
 */
struct mps2_i2c_bus {
  MPS2_I2C_TypeDef *regs;     /**< Pointer to MPS2 I2C peripheral registers */
  uint32_t delay_cycles;      /**< Software timing parameter for bit delay */
  uint32_t timeout_cycles;    /**< Clock stretching timeout limit */
  
  /**
   * Simulation mode flag
   * 1: Simulate ideal bus behavior (for QEMU without physical I2C device)
   *    - SCL always reads high
   *    - SDA always reads high
   *    - Slave always ACKs
   * 0: Read actual MPS2 I2C peripheral line status
   */
  uint8_t  simulate_bus;
};

struct mps2_i2c_msg {
  uint8_t *buf;    /**< Data buffer */
  size_t len;      /**< Number of bytes */
  uint8_t flags;   /**< Message flags (READ/WRITE/STOP/RESTART) */
};

int mps2_i2c_init(struct mps2_i2c_bus *bus);

int mps2_i2c_transfer(struct mps2_i2c_bus *bus,
		struct mps2_i2c_msg *messages,
		size_t num_messages,
	        uint8_t target_addr);

int mps2_i2c_probe(struct mps2_i2c_bus *bus, uint8_t target_addr);

int mps2_i2c_recover_bus(struct mps2_i2c_bus *bus);

#endif
