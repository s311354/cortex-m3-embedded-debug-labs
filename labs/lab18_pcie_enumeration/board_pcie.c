#include "system_CM3DS.h"

#include "board_pcie.h"
#include "pcie_sim_host.h"

static struct pcie_sim_controller g_sim_controller;

struct pcie_host g_board_pcie_host = {
    .ops = &g_pcie_sim_host_ops,
    .priv = &g_sim_controller
};

int board_pcie_init(void) {
    if (SystemCoreClock == 0U)
	return PCIE_ERR_PLATFORM;

    pcie_sim_controller_init(&g_sim_controller);

    return PCIE_OK;
}
