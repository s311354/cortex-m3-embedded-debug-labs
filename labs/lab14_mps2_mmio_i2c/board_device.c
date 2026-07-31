#include "board_device.h"


/*
 * 1 iteration ~ 5 CPU cycles. CPU = 25 MHZ, 1 us = 25 cycles. 
 * I2C clock = 10 us, delay_cycles ~ 10 / 2 * 25 / 5 = 25 iterations
 */
struct mps2_i2c_bus g_shield0_i2c_bus = {
    .regs = MPS2_SHIELD0_I2C,
    .delay_cycles = 25U,
    .timeout_cycles = 10000U,

#if defined(LAB14_REAL_EEPROM)
    .simulate_bus = 0U
#else
    .simulate_bus = 1U
#endif
};

/*
 *  Board-level device declaration.
 *
 *  0x50 is a 7-bit I2C slave adress.
 *  It is not a CPU MMIO address and therefore should not be placed in
 *  CM3DS_MPS2.h or SMM_MPS2.h.
 */

const struct eeprom_device g_board_eeprom = {
  .bus = &g_shield0_i2c_bus,
  .target_addr = 0x50U,
  .address_width = 1U,
  .page_size = 8U,
  .ready_poll_limit = 1000U
};

int board_devices_init(void) {
    return mps2_i2c_init(&g_shield0_i2c_bus);
}
