#ifndef BOARD_PCIE_H
#define BOARD_PCIE_H

#include "pcie_host.h"

extern struct pcie_host g_board_pcie_host;

int board_pcie_init(void);

#endif
