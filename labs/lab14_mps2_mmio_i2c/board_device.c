#include "board_device.h"


/**
 * MPS2 Shield 0 I2C bus configuration
 * 
 * Timing calculation:
 * - Target I2C speed: ~100 kHz (Standard Mode)
 * - I2C bit time: 10 μs (100 kHz)
 * - Half bit time: 5 μs
 * - CPU frequency: 48 MHz (assumed)
 * - Cycles per μs: 48
 * - Cycles per half-bit: 240
 * - Accounting for function overhead: delay_cycles ≈ 48
 * 
 * The delay_cycles value can be tuned based on actual timing measurements.
 */
struct mps2_i2c_bus g_shield0_i2c_bus = {
    .regs = MPS2_SHIELD0_I2C,
    .delay_cycles = 48U,
    .timeout_cycles = 10000U,

#if defined(LAB14_REAL_EEPROM)
    .simulate_bus = 0U  /* Real hardware: read actual MPS2 I2C peripheral state */
#else
    .simulate_bus = 1U  /* QEMU simulation: fake bus responses */
#endif
};

/**
 * Address 0x50 is the standard 7-bit I2C address for many I2C EEPROMs
 * (24C02, 24C04, etc.). This is a device address on the I2C bus, NOT
 * a memory-mapped CPU address.
 */
const struct eeprom_device g_board_eeprom = {
  .bus = &g_shield0_i2c_bus,
  .target_addr = 0x50U,         /* 7-bit I2C slave address */
  .address_width = 1U,          /* 1 byte internal address (e.g., 24C02) */
  .page_size = 8U,              /* 8-byte write page size */
  .ready_poll_limit = 1000U     /* Maximum ACK polling attempts after write */
};

int board_devices_init(void) {
    return mps2_i2c_init(&g_shield0_i2c_bus);
}
