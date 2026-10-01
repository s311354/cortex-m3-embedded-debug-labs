#include <stdint.h>
#include "board_device.h"
#include "eeprom.h"
#include "mps2_i2c.h"
#include "device.h"

enum lab14_stage {
    LAB14_STAGE_RESET = 0,
    LAB14_STAGE_BOARD_INIT,
    LAB14_STAGE_WRITE,
    LAB14_STAGE_READ,
    LAB14_STAGE_COMPARE,
    LAB14_STAGE_DONE,
    LAB14_STAGE_ERROR,
    LAB14_STAGE_PROBE
};

volatile enum lab14_stage g_lab14_stage;

volatile int g_board_init_result;
volatile int g_i2c_probe_result;
volatile int g_eeprom_write_result;
volatile int g_eeprom_read_result;
volatile int g_eeprom_compare_result;
volatile uint32_t g_i2c_last_control;
volatile uint16_t g_test_memory_addr;
volatile uint8_t g_test_write_value;
volatile uint8_t g_eeprom_read_value;

void lab14_debug_chechpoint(void) {
    __NOP();
}

__attribute__((naked))
void SVC_Handler(void) {
  __asm volatile ("bx lr");
}

int main(void) {
    uint8_t read_value = 0U;

    g_lab14_stage = LAB14_STAGE_RESET;
    g_test_memory_addr = 0x0010U;
    g_test_write_value = 0xABU;
    g_eeprom_read_value = 0U;

    g_lab14_stage = LAB14_STAGE_BOARD_INIT;
    lab14_debug_chechpoint();

    g_board_init_result = board_devices_init();

    if (g_board_init_result != MPS2_I2C_OK) {
	goto error;
    }

    g_lab14_stage = LAB14_STAGE_PROBE;
    lab14_debug_chechpoint();

    g_i2c_probe_result = mps2_i2c_probe(g_board_eeprom.bus, g_board_eeprom.target_addr);

    if (g_i2c_probe_result != MPS2_I2C_OK)
	goto error;

    g_lab14_stage = LAB14_STAGE_WRITE;
    lab14_debug_chechpoint();

    g_eeprom_write_result = eeprom_write_byte(&g_board_eeprom, g_test_memory_addr, g_test_write_value);

    if (g_eeprom_write_result != EEPROM_OK) {
	goto error;
    }

    g_lab14_stage = LAB14_STAGE_READ;
    lab14_debug_chechpoint();

    g_eeprom_read_result = eeprom_read_byte(&g_board_eeprom, g_test_memory_addr, &read_value);

    g_eeprom_read_value = read_value;

    if (g_eeprom_read_result != EEPROM_OK) {
	goto error;
    }

    g_lab14_stage = LAB14_STAGE_COMPARE;
    lab14_debug_chechpoint();

    g_eeprom_compare_result = (g_eeprom_read_value == g_test_write_value) ? 0 : -1;

    if (g_eeprom_compare_result != 0)
	goto error;

    g_lab14_stage = LAB14_STAGE_DONE;
    goto finished;

error:
    g_lab14_stage = LAB14_STAGE_ERROR;

finished:
    g_i2c_last_control = g_board_i2c_bus.regs->CONTROL;

    lab14_debug_chechpoint();

    while (1) {
	__NOP();
    }

}
