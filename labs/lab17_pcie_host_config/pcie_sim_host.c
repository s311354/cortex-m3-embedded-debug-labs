#include <stddef.h>
#include <stdint.h>

#include "pcie_sim_host.h"

#define SIM_VENDOR_ID    0x1234U
#define SIM_DEVICE_ID    0x5678U

#define SIM_CLASS_CODE   0x02U
#define SIM_SUBCLASS     0x00U
#define SIM_PROG_IF      0x00U
#define SIM_REVISION     0x01U

#define SIM_BAR0         0x50000000U

static int bdf_equal(struct pcie_bdf lhs, struct pcie_bdf rhs) {
    return (lhs.bus == rhs.bus) && (lhs.device == rhs.device) && (lhs.function == rhs.function);
}

static int sim_config_read32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t *value) {
    struct pcie_sim_controller *controller;

    controller = (struct pcie_sim_controller *) host->priv;

    if ((controller == NULL) || (value == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    if (!bdf_equal(bdf, controller->endpoint.bdf)) {
        *value = 0xFFFFFFFFU;
	return PCIE_ERR_NO_DEVICE;
    }

    *value = controller->endpoint.config[offset >> 2U];
    
    return PCIE_OK;
}

static int sim_config_write32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t value) {
    struct pcie_sim_controller *controller;

    controller = (struct pcie_sim_controller *)host->priv;

    if (controller == NULL)
        return PCIE_ERR_ARGUMENT;

    if (!bdf_equal(bdf, controller->endpoint.bdf)) {
        return PCIE_ERR_NO_DEVICE;
    }


    controller->endpoint.config[offset >> 2U] = value;

    return PCIE_OK;
}

const struct pcie_host_ops g_pcie_sim_host_ops = {
    .config_read32 = sim_config_read32,
    .config_write32 = sim_config_write32
};

void pcie_sim_controller_init(struct pcie_sim_controller *controller) {
    if (controller == NULL)
        return;
    
    controller->endpoint.bdf.bus = 0U;
    controller->endpoint.bdf.device = 1U;
    controller->endpoint.bdf.function = 0U;

    for (size_t index = 0U; index < PCIE_SIM_CONFIG_DWORDS; ++index) {
        controller->endpoint.config[index] = 0U;
    }

    /*
     * 0x00
     * [32:16]: Device ID
     * [15:0]:  Vendor ID
     */
    controller->endpoint.config[0x00U >> 2U] = ((uint32_t)SIM_DEVICE_ID << 16U) | SIM_VENDOR_ID;

    /*
     * 0x04
     */
    controller->endpoint.config[0x04U >> 2U] = 0x00000000U;

    /*
     * 0x08
     */
    controller->endpoint.config[0x08 >> 2U] = ((uint32_t)SIM_CLASS_CODE << 24U) | ((uint32_t)SIM_SUBCLASS << 16U) | ((uint32_t) SIM_PROG_IF << 8U) | SIM_REVISION;

    /*
     * Header Type = 0
     */
    controller->endpoint.config[0x0CU >> 2U] = 0x00000000U;

    controller->endpoint.config[PCIE_BAR0_OFFSET >> 2U] = SIM_BAR0;
}
