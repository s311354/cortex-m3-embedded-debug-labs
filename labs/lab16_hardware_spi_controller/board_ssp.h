#ifndef BOARD_SSP_H
#define BOARD_SSP_H

#include "mps2_ssp.h"

extern struct mps2_ssp g_board_ssp3;

extern const struct mps2_ssp_config g_board_ssp3_config;

void board_ssp3_reset_runtime_state(void);

#endif
