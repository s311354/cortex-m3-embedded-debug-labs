#include "system_CM3DS.h"

#include "board_pcie.h"
#include "pcie_sim_host.h"

static struct lab_pcie_sim_controller g_sim_controller;

struct lab_pcie_host g_board_pcie_host = {
    .ops = &g_lab_pcie_sim_host_ops,

    .priv = &g_sim_controller
};

struct lab_pcie_resource_window g_board_pcie_mem_window;

int board_pcie_init(void) {
    int result;

    if (SystemCoreClock == 0U)
	return LAB_PCIE_ERR_PLATFORM;

    lab_pcie_sim_controller_init(&g_sim_controller);

    result = lab_pcie_resource_window_init(&g_board_pcie_mem_window,
		                           /* PCI bus address */
		                           0x50000000U,
					   /* CPU physical address */
					   0x60000000U,
					   /* 16 MiB */
					   0x01000000U);

    return result;
}
