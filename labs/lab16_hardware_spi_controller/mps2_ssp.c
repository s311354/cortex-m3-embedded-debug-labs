#include "mps2_ssp.h"


static uint32_t build_cr0(const struct mps2_ssp_config *cfg) {
    uint32_t value;

    value = ((cfg->data_bits - 1U) << SSP_CR0_DSS_Pos);

    value |= SSP_CR0_FRF_MOT;

    value |= ((uint32_t)cfg->scr << SSP_CR0_SCR_Pos);

    switch (cfg->mode) {
        case MPS2_SSP_MODE_0:
		break;
	case MPS2_SSP_MODE_1:
		value |= SSP_CR0_SPH_Msk;
		break;
	case MPS2_SSP_MODE_2:
		value |= SSP_CR0_SPO_Msk;
		break;
	case MPS2_SSP_MODE_3:
		value |= SSP_CR0_SPO_Msk | SSP_CR0_SPH_Msk;
		break;
	default:
		break;
    }

    return value;
}

int mps2_ssp_init(struct mps2_ssp *ssp, const struct mps2_ssp_config *cfg) {
    if (ssp == 0 || cfg == 0) {
        return MPS2_SSP_ERROR_ARGUMENT;
    }

    // Disable SSP
    ssp->regs->CR1 &= ~SSP_CR1_SSE_Msk;

    // Disable interrupt/DMA
    ssp->regs->IMSC = 0;
    ssp->regs->DMACR = 0;

    // Clock divider
    ssp->regs->CPSR = cfg->cpsdvsr;

    // SPI mode
    ssp->regs->CR0 = build_cr0(cfg);

    uint32_t cr1 = 0;

    if (cfg->loopback) {
        cr1 |= SSP_CR1_LBM_Msk;
    }

    // Master mode
    cr1 &= ~SSP_CR1_MS_Msk;

    // Enable SSP
    cr1 |= SSP_CR1_SSE_Msk;

    ssp->regs->CR1 = cr1;

    ssp->actual_clock_hz = cfg->input_clock_hz / (cfg->cpsdvsr * (cfg->scr + 1));

    return MPS2_SSP_OK;
}

static int wait_flag(struct mps2_ssp *ssp, uint32_t flag) {
    uint32_t timeout = ssp->timeout_cycles;

    while ((ssp->regs->SR & flag) == 0) {
        if (timeout-- == 0) {
	    return -1;
	}
    }

    return 0;
}

int mps2_ssp_transfer(struct mps2_ssp *ssp, const volatile uint8_t *tx, volatile uint8_t *rx, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (wait_flag(ssp, SSP_SR_TNF_Msk)) {
	    return MPS2_SSP_ERROR_TX_TIMEOUT;
	}

        // TX FIFO write
        ssp->regs->DR = tx[i];
        
        if (wait_flag(ssp, SSP_SR_RNE_Msk)) {
            return MPS2_SSP_ERROR_RX_TIMEOUT;
        }
    
        // RX FIFO read
        rx[i] = ssp->regs->DR & 0xff;
    }

    return MPS2_SSP_OK;
}

int mps2_ssp_wait_idle(struct mps2_ssp *ssp) {
    uint32_t timeout = ssp->timeout_cycles;

    while (ssp->regs->SR & SSP_SR_BSY_Msk) {
        if (timeout-- == 0) {
	    return MPS2_SSP_ERROR_BUSY_TIMEOUT;
	}
    }

    return MPS2_SSP_OK;
}

void mps2_ssp_enable_interrupts(struct mps2_ssp *ssp, uint32_t mask) {
    ssp->regs->IMSC |= mask;
}

void mps2_ssp_disable_interrupts(struct mps2_ssp *ssp, uint32_t mask) {
    ssp->regs->IMSC &= ~mask;
}

