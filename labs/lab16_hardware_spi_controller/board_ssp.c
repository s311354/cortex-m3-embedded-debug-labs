#include "board_ssp.h"

#define BOARD_SSP_INPUT_CLOCK_HZ 25000000UL
#define BOARD_SSP_TIMEOUT_CYCLES 100000UL

/*
 * CPSR default:
 *
 * CPSDVSR = 8
 * CR0 SCR: SCR = 3
 *
 * SPI clock: 25MHz / (8 * (3 + 1)) = 781250 Hz
 */

#define BOARD_SSP_CPSDVSR \
	SSP_CPSR_DFLT

#define BOARD_SSP_SCR \
	((SSP_CR0_SCR_DFLT & SSP_CR0_SCR_Msk) >> \
	 SSP_CR0_SCR_Pos)

struct mps2_ssp g_board_ssp3 = 
{
	.regs = MPS2_SSP3,
	.timeout_cycles = BOARD_SSP_TIMEOUT_CYCLES,
	.actual_clock_hz = 0U
};

const struct mps2_ssp_config g_board_ssp3_config =
{
	.mode = MPS2_SSP_MODE_0,
	.data_bits = 8U,
	.loopback = 1U, // PL022 internal loopback
	.cpsdvsr = BOARD_SSP_CPSDVSR,
	.scr = BOARD_SSP_SCR,
	.input_clock_hz = BOARD_SSP_INPUT_CLOCK_HZ
};

void board_ssp3_reset_runtime_state(void) {
    g_board_ssp3.actual_clock_hz = 0U;
}
