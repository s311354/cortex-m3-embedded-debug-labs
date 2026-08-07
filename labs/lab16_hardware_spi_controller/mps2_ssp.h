#ifndef MPS2_SSP_H
#define MPS2_SSP_H

#include <stdint.h>
#include <stddef.h>

#include "SMM_MPS2.h"

enum mps2_ssp_mode {
    MPS2_SSP_MODE_0 = 0,
    MPS2_SSP_MODE_1,
    MPS2_SSP_MODE_2,
    MPS2_SSP_MODE_3
};

enum mps2_ssp_result {
    MPS2_SSP_OK = 0,
    MPS2_SSP_ERROR_ARGUMENT = -1,
    MPS2_SSP_ERROR_CLOCK = -2,
    MPS2_SSP_ERROR_TX_TIMEOUT = -3,
    MPS2_SSP_ERROR_RX_TIMEOUT = -4,
    MPS2_SSP_ERROR_BUSY_TIMEOUT = -5
};

struct mps2_ssp_config {
    enum mps2_ssp_mode mode;

    uint8_t data_bits;
    uint8_t loopback;
    uint16_t cpsdvsr;
    uint8_t scr;
    uint32_t input_clock_hz;
};

struct mps2_ssp {
    MPS2_SSP_TypeDef *regs;

    uint32_t timeout_cycles;
    uint32_t actual_clock_hz;
};

int mps2_ssp_init(struct mps2_ssp *ssp, const struct mps2_ssp_config *config);

int mps2_ssp_transfer(struct mps2_ssp *ssp, const volatile uint8_t *tx, volatile uint8_t *rx, size_t length);

int mps2_ssp_wait_idle(struct mps2_ssp *ssp);

void mps2_ssp_enable_interrupts(struct mps2_ssp *ssp, uint32_t mask);

void mps2_ssp_disable_interrupts(struct mps2_ssp *ssp, uint32_t mask);

#endif
