#include <stdint.h>
#include "i2c_message.h"
#include "device.h"

__attribute__((naked))
void SVC_Handler(void) {

}

#define EEPROM_TARGET_ADDR  0x50U
#define TEST_REGISTER_ADDR  0x10U
#define TEST_REGISTER_VALUE 0xABU

volatile int g_i2c_result;

int main(void) {

    uint8_t tx_data[] = {
        TEST_REGISTER_ADDR,
	TEST_REGISTER_VALUE
    };

    struct i2c_msg msg = {
        .buf = tx_data,
	.len = sizeof(tx_data),
	.flags = I2C_MSG_WRITE |
		 I2C_MSG_STOP,
    };

    g_i2c_result = i2c_transfer_bitbang(EEPROM_TARGET_ADDR, &msg, 1U);

    while (1) {
    }
}
