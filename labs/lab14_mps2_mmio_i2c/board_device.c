#include "board_device.h"
#include "system_CM3DS.h"

#define BOARD_I2C_TARGET_HZ 100000U
#define BOARD_DELAY_LOOP_CYCLES 5U

struct mps2_i2c_bus g_board_i2c_bus = {
    .regs = MPS2_SHIELD1_I2C,
    .delay_cycles = 0U,
    .timeout_cycles = 10000U,
};


/**
 * Address 0x50 is the standard 7-bit I2C address for many I2C EEPROMs.
 * The QEMU at24c-eeprom model uses a two-byte internal address.
 */
const struct eeprom_device g_board_eeprom = {
  .bus = &g_board_i2c_bus,
  .target_addr = 0x50U,         /* 7-bit I2C slave address */
  .address_width = 2U,          /* QEMU at24c-eeprom uses a 16-bit address */
  .page_size = 8U,              /* 8-byte write page size */
  .ready_poll_limit = 1000U     /* Maximum ACK polling attempts after write */
};

/**
 * MPS2 Shield 1 I2C bus configuration
 * 
 * Timing calculation:
 * - Target I2C speed: ~100 kHz (Standard Mode)
 * - I2C bit time: 10 μs (100 kHz)
 * - Half bit time: 5 μs
 * - CPU frequency: SystemCoreClock
 */
static uint32_t board_i2c_delay_cycles(uint32_t core_clock_hz, uint32_t i2c_clock_hz) {
    uint32_t half_period_cycles;

    if ((core_clock_hz == 0U) || (i2c_clock_hz == 0U)) {
        return 0U;
    }

    half_period_cycles = core_clock_hz / (2U * i2c_clock_hz);

    return half_period_cycles / BOARD_DELAY_LOOP_CYCLES;
}

int board_devices_init(void) {
    g_board_i2c_bus.delay_cycles = board_i2c_delay_cycles(SystemCoreClock, BOARD_I2C_TARGET_HZ);

    if (g_board_i2c_bus.delay_cycles == 0U)
        return MPS2_I2C_ERR_ARGUMENT;

    return mps2_i2c_init(&g_board_i2c_bus);
}
