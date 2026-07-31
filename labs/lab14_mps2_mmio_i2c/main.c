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
    LAB14_STAGE_ERROR
};

volatile enum lab14_stage g_lab14_stage;

volatile int g_board_init_result;
volatile int g_eeprom_write_result;
volatile int g_eeprom_read_result;
volatile int g_eeprom_compare_result;

volatile uint16_t g_test_memory_addr;
volatile uint8_t g_test_write_value;
volatile uint8_t g_eeprom_read_value;

__attribute__((noinline))
void lab14_debug_chechpoint(void) {
    __asm volatile ("nop");
}

__attribute__((naked))
void SVC_Handler(void) {
  __asm volatile ("bx lr");
}

int main(void) {

    uint8_t read_value;

    g_lab14_stage = LAB14_STAGE_RESET;
    g_test_memory_addr = 0x0010U;
    g_test_write_value = 0xABU;
    g_eeprom_read_value = 0U;

    g_lab14_stage = LAB14_STAGE_BOARD_INIT;
    lab14_debug_chechpoint();
    g_board_init_result = board_devices_init();

    if (g_board_init_result != MPS2_I2C_OK) {
        g_lab14_stage = LAB14_STAGE_ERROR;
	lab14_debug_chechpoint();

	while (1) {
	}
    }

    g_lab14_stage = LAB14_STAGE_WRITE;
    lab14_debug_chechpoint();

    g_eeprom_write_result = eeprom_write_byte(&g_board_eeprom, g_test_memory_addr, g_test_write_value);

    if (g_eeprom_write_result != EEPROM_OK) {
        g_lab14_stage = LAB14_STAGE_ERROR;
	lab14_debug_chechpoint();

	while (1) {
	}
    }

    read_value = 0U;
    g_lab14_stage = LAB14_STAGE_READ;
    lab14_debug_chechpoint();

    g_eeprom_read_result = eeprom_read_byte(&g_board_eeprom, g_test_memory_addr, &read_value);

    g_eeprom_read_value = read_value;

    if (g_eeprom_read_result != EEPROM_OK) {
        g_lab14_stage = LAB14_STAGE_ERROR;
	lab14_debug_chechpoint();

	while (1) {
	}
    }

    g_lab14_stage = LAB14_STAGE_COMPARE;

#if defined(LAB14_REAL_EERPOM)
    g_eeprom_compare_result = (g_eeprom_read_value == g_test_write_value) ? 0 : -1;
#else
    g_eeprom_compare_result = (g_eeprom_read_value == 0xFFU) ? 0 : -1;
#endif

    g_lab14_stage = (g_eeprom_compare_result == 0) ? LAB14_STAGE_DONE : LAB14_STAGE_ERROR;

    lab14_debug_chechpoint();

    while (1) {
    }

}
