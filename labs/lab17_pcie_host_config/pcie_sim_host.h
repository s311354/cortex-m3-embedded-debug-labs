#ifndef PCIE_SIM_HOST_H
#define PCIE_SIM_HOST_H

#include "pcie_host.h"

#define PCIE_SIM_CONFIG_DWORDS 64U

struct pcie_sim_function {
    struct pcie_bdf bdf;

    /* 256-byte PCI configuration header.
     */
    volatile uint32_t config[PCIE_SIM_CONFIG_DWORDS];
};

struct pcie_sim_controller {
    struct pcie_sim_function endpoint;
};

extern const struct pcie_host_ops g_pcie_sim_host_ops;

void pcie_sim_controller_init(struct pcie_sim_controller *controller);

#endif
