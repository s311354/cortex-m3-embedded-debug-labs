#ifndef PCIE_SIM_HOST_H
#define PCIE_SIM_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "pcie_host.h"

#define PCIE_SIM_CONFIG_DWORDS    64U
#define PCIE_SIM_MAX_FUNCTIONS    4U

struct pcie_sim_function {
    struct pcie_bdf bdf;

    uint8_t present;

    volatile uint32_t config[PCIE_SIM_CONFIG_DWORDS];
};

struct pcie_sim_controller {
    struct pcie_sim_function functions[PCIE_SIM_MAX_FUNCTIONS];

    size_t function_count;
};

extern const struct pcie_host_ops g_pcie_sim_host_ops;

void pcie_sim_controller_init(struct pcie_sim_controller *controller);

#endif
