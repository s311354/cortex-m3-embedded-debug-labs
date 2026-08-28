#ifndef LAB19_PCIE_SIM_HOST_H
#define LAB19_PCIE_SIM_HOST_H

#include <stddef.h>
#include <stdint.h>

#include "pcie_host.h"

#define LAB_PCIE_SIM_CONFIG_DWORDS 64U
#define LAB_PCIE_SIM_MAX_FUNCTIONS 1U
#define LAB_PCIE_SIM_BAR_COUNT     6U

struct lab_pcie_sim_bar {
    uint32_t value;
    uint32_t size;

    uint8_t implemented;
    uint8_t probing;
};

struct lab_pcie_sim_function {
    pcie_bdf_t bdf;

    uint8_t present;

    volatile uint32_t config[LAB_PCIE_SIM_CONFIG_DWORDS];

    struct lab_pcie_sim_bar bars[LAB_PCIE_SIM_BAR_COUNT];
};

struct lab_pcie_sim_controller {
    struct lab_pcie_sim_function functions[LAB_PCIE_SIM_MAX_FUNCTIONS];

    size_t function_count;
};

extern const struct lab_pcie_host_ops g_lab_pcie_sim_host_ops;

void lab_pcie_sim_controller_init(struct lab_pcie_sim_controller *controller);

#endif
