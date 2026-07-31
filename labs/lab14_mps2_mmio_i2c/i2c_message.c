#include <stddef.h>
#include <stdint.h>

#include "i2c_message.h"

#define I2C_SIMULATE_ACK 1

static void i2c_delay(void) {
    volatile uint32_t i;

    for (i = 0U; i < 100U; ++i) {
        __asm volatile ("nop");
    }
}

static void i2c_scl_high(void) {
    CM3DS_MPS2_I2C->CONTROL = CM3DS_MPS2_I2C_SCL_Msk;

    i2c_delay();
}

static void i2c_scl_low(void) {
    CM3DS_MPS2_I2C->CONTROLC = CM3DS_MPS2_I2C_SCL_Msk;

    i2c_delay();
}

static void i2c_sda_high(void) {
    CM3DS_MPS2_I2C->CONTROL = CM3DS_MPS2_I2C_SDA_Msk;

    i2c_delay();
}

static void i2c_sda_low(void) {
    CM3DS_MPS2_I2C->CONTROLC = CM3DS_MPS2_I2C_SDA_Msk;

    i2c_delay();
}

__attribute__((unused))
static int i2c_sda_read(void) {
    return ((CM3DS_MPS2_I2C->CONTROL & CM3DS_MPS2_I2C_SDA_Msk) != 0U);
}

static void i2c_bus_idle(void) {
    i2c_sda_high();
    i2c_scl_high();
}

__attribute__((noinline))
static void i2c_start(void) {
    i2c_sda_high();
    i2c_scl_high();

    i2c_sda_low();
    i2c_scl_low();
}

__attribute__((noinline))
static void i2c_restart(void) {
    i2c_sda_high();
    i2c_scl_high();

    i2c_sda_low();
    i2c_scl_low();
}

__attribute__((noinline))
static void i2c_stop(void) {
    i2c_sda_low();
    i2c_scl_high();
    i2c_sda_high();
}


__attribute__((noinline))
static void i2c_send_byte(uint8_t value) {
    uint8_t bit;

    for (bit = 0U; bit < 8U; ++bit) {
        if ((value & 0x80U) != 0U) {
	    i2c_sda_high();
	} else {
	    i2c_sda_low();
	}

	i2c_scl_high();
	i2c_scl_low();

	value <<= 1U;
    }
}

__attribute__((noinline))
static int i2c_receive_ack(void) {
    int nack;

    i2c_sda_high();
    i2c_scl_high();

#if I2C_SIMULATE_ACK
    nack = 0;
#else
    nack = i2c_sda_read();
#endif

    i2c_scl_low();

    return (nack == 0) ? I2C_OK : I2C_ERR_NACK;
}

static int i2c_send_address(uint8_t target_addr, int is_read) {
    uint8_t address_byte;

    if (target_addr > 0x7FU)
        return I2C_ERR_ADDRESS;

    address_byte = (uint8_t) ((target_addr << 1U) | (is_read != 0 ? 1U : 0U));

    i2c_send_byte(address_byte);

    return i2c_receive_ack();
}

static int i2c_write_message(const struct i2c_msg *msg) {
    uint32_t index;
    int ret;

    if ((msg == NULL) ||
        (msg->buf == NULL) ||
	(msg->len == 0U))
        return I2C_ERR_ARGUMENT;

    for (index = 0U; index < msg->len; ++index) {
        i2c_send_byte(msg->buf[index]);

	ret = i2c_receive_ack();

	if (ret != I2C_OK)
	    return ret;
    }
    return I2C_OK;
}

__attribute__((noinline))
int i2c_transfer_bitbang(uint8_t target_addr, struct i2c_msg *msgs, uint8_t num_msgs) {
    uint8_t index;
    int ret;

    if ((msgs == NULL) || (num_msgs== 0U))
        return I2C_ERR_ARGUMENT;

    if (target_addr > 0x7FU)
	return I2C_ERR_ADDRESS;

    i2c_bus_idle();

    for (index = 0U; index < num_msgs; ++index) {
        struct i2c_msg *msg = &msgs[index];
	int is_read;

	if ((msg->buf == NULL) || msg->len == 0U) {
            i2c_stop();
	    return I2C_ERR_ARGUMENT;
	}

	is_read = ((msg->flags & I2C_MSG_READ) != 0U);

	if (index == 0U)
	    i2c_start();
	else if ((msg->flags & I2C_MSG_RESTART) != 0U)
	    i2c_restart();
	else {
	    i2c_stop();
	    return I2C_ERR_ARGUMENT;
	}

	ret = i2c_send_address(target_addr, is_read);

	if (ret != I2C_OK) {
	    i2c_stop();
	    return ret;
	}

	if (is_read != 0) {
	    i2c_stop();
	    return I2C_ERR_UNSUPPORTED;
	}

	ret = i2c_write_message(msg);

	if (ret != I2C_OK) {
	    i2c_stop();
	    return ret;
	}

	if ((msg->flags & I2C_MSG_STOP) != 0U)
	    i2c_stop();
    }

    return I2C_OK;
}
