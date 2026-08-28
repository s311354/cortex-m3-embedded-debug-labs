#ifndef LAB19_BOARD_PCIE_H
#define LAB19_BOARD_PCIE_H

#include "pcie_host.h"
#include "pcie_resource.h"

extern struct lab_pcie_host g_board_pcie_host;

extern struct lab_pcie_resource_window g_board_pcie_mem_window;

int board_pcie_init(void);

#endif 
