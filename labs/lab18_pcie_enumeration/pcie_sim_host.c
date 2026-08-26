#include <stddef.h>
#include <stdint.h>

#include "pcie_sim_host.h"

static struct pcie_sim_function *sim_find_function(struct pcie_sim_controller *controller, struct pcie_bdf bdf) {
    for (size_t index = 0; index < controller->function_count; ++index) {
        struct pcie_sim_function *fn = &controller->functions[index];

	if ((fn->present != 0U) && (fn->bdf.bus == bdf.bus) && (fn->bdf.device == bdf.device) && (fn->bdf.function == bdf.function)) {
	    return fn;
	}
    }

    return NULL;
}

static int sim_config_read32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t *value) {
    struct pcie_sim_controller *controller;
    struct pcie_sim_function *fn;

    if ((host == NULL) || (host->priv == NULL) || (value == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    controller = (struct pcie_sim_controller *) host->priv;

    fn = sim_find_function(controller, bdf);

    if (fn == NULL) {
        *value = 0xFFFFFFFFU;
	return PCIE_OK;
    }

    *value = fn->config[offset >> 2U];

    return PCIE_OK;
}

static int sim_config_write32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t value) {
    struct pcie_sim_controller *controller;
    struct pcie_sim_function *fn;

    if ((host == NULL) || (host->priv == NULL))
        return PCIE_ERR_ARGUMENT;

    controller = (struct pcie_sim_controller *) host->priv;

    fn = sim_find_function(controller, bdf);

    if (fn == NULL)
	return PCIE_OK;

    fn->config[offset >> 2U] = value;

    return PCIE_OK;
}

const struct pcie_host_ops g_pcie_sim_host_ops = {
    .config_read32 = sim_config_read32,
    .config_write32 = sim_config_write32
};

static void sim_init_function(struct pcie_sim_function *fn, uint8_t bus, uint8_t device, uint8_t function, 
		              uint16_t vendor, uint16_t device_id, uint8_t class_code, uint8_t subclass,
			      uint8_t header_type, uint32_t bar0, uint8_t irq_line, uint8_t irq_pin) {

    fn->bdf.bus = bus;
    fn->bdf.device = device;
    fn->bdf.function = function;

    fn->present = 1U;

    for (size_t index = 0; index < PCIE_SIM_CONFIG_DWORDS; ++index) {
        fn->config[index] = 0U;
    }

    fn->config[PCIE_CFG_VENDOR_DEVICE >> 2U] = ((uint32_t) device_id << 16U) | vendor;

    fn->config[PCIE_CFG_CLASS_REVISION >> 2U] = ((uint32_t) class_code << 24U) | ((uint32_t) subclass << 16U) | 0x01U;

    fn->config[PCIE_CFG_BAR0 >> 2U] = bar0;

    fn->config[PCIE_CFG_INTERRUPT >> 2U] = ((uint32_t) irq_pin << 8U) | irq_line;
}

void pcie_sim_controller_init(struct pcie_sim_controller *controller) {
    if (controller == NULL)
	return;

    controller->function_count = 4U;

    sim_init_function(&controller->functions[0], 0U, 1U, 0U, 
		       0x1234U, 0x5678U, 0x02U, 0x00U, 
		       0x00U, 0x50000000U, 5U, 1U);

    sim_init_function(&controller->functions[1], 0U, 3U, 0U, 
    	               0xABCU, 0x1000U, 0x01U, 0x06U, 
    		       0x00U, 0x51000000U, 7U, 1U);

    sim_init_function(&controller->functions[2], 0U, 5U, 0U, 
     	               0xCAFEU, 0x0001U, 0x04U, 0x01U, 
     		       0x80U, 0x52000000U, 9U, 1U);

    sim_init_function(&controller->functions[3], 0U, 5U, 0U, 
     	               0xCAFEU, 0x0002U, 0x03U, 0x00U, 
     		       0x00U, 0x53000000U, 10U, 1U);
}
