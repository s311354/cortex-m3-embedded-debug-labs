#ifndef I2C_MESSAGE_H
#define I2C_MESSAGE_H

#include <stdint.h>
#include "CM3DS_MPS2.h"

#define I2C_MSG_WRITE    0x00U
#define I2C_MSG_READ     0x01U
#define I2C_MSG_STOP     0x02U
#define I2C_MSG_RESTART  0x04U

#define I2C_OK               0
#define I2C_ERR_ARGUMENT    -1
#define I2C_ERR_ADDRESS     -2
#define I2C_ERR_NACK        -3
#define I2C_ERR_UNSUPPORTED -4

struct i2c_msg {
    uint8_t *buf;
    uint32_t len;
    uint8_t flags;
};

int i2c_transfer_bitbang(uint8_t target_addr, struct i2c_msg *msg, uint8_t num_msgs);

#endif
