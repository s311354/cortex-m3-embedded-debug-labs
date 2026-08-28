#include <stddef.h>
#include <stdint.h>

#include "pcie_sim_host.h"

static struct lab_pcie_sim_function * sim_find_function(struct lab_pcie_sim_controller *controller, pcie_bdf_t bdf) {
    for (size_t index = 0; index < controller->function_count; ++index) {
        struct lab_pcie_sim_function *fn = &controller->functions[index];

	if ((fn->present != 0U) && (fn->bdf == bdf)) {
	    return fn;
	}
    }

    return NULL;
}

static int sim_bar_index(unsigned int reg) {
    if ((reg < PCIE_CONF_BAR0) || (reg > PCIE_CONF_BAR5))
	return -1;

    return (int) (reg - PCIE_CONF_BAR0);
}

static uint32_t sim_bar_probe_value(const struct lab_pcie_sim_bar *bar) {
    uint32_t address_mask;
    uint32_t flags;

    if ((bar == NULL) || (bar->implemented == 0U) || (bar->size == 0U))
	return 0U;

    if (PCIE_CONF_BAR_IO(bar->value)) {
        address_mask = ~(bar->size - 1U) & ~0x3U;

	flags = bar->value & 0x3U;
    } else {
        address_mask = ~(bar->size - 1U) & ~0xFU;
	
	flags = PCIE_CONF_BAR_FLAGS(bar->value);
    }

    return address_mask | flags;
}

static int sim_config_read32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t *value) {
    struct lab_pcie_sim_controller *controller;
    struct lab_pcie_sim_function *fn;

    if ((host == NULL) || (host->priv == NULL) || (value == NULL))
	return LAB_PCIE_ERR_ARGUMENT;

    controller = (struct lab_pcie_sim_controller *) host->priv;

    fn = sim_find_function(controller, bdf);

    if (fn == NULL) {
        *value = 0xFFFFFFFFU;

	return LAB_PCIE_OK;
    }

    int bar_index = sim_bar_index(reg);

    if (bar_index >= 0) {
        struct lab_pcie_sim_bar *bar = &fn->bars[bar_index];

	if (bar->implemented == 0U) {
	    *value = 0U;
	    return LAB_PCIE_OK;
	}

	if (bar->probing != 0U) {
            *value = sim_bar_probe_value(bar);
	    return LAB_PCIE_OK;
	}

	*value = bar->value;

	return LAB_PCIE_OK;
    }

    *value = fn->config[reg];

    return LAB_PCIE_OK;
}

static int sim_config_write32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t value) {
    struct lab_pcie_sim_controller *controller;
    struct lab_pcie_sim_function *fn;

    if ((host == NULL) || (host->priv == NULL))
	return LAB_PCIE_ERR_ARGUMENT;

    controller = (struct lab_pcie_sim_controller *) host->priv;

    fn = sim_find_function(controller, bdf);

    if (fn == NULL)
	return LAB_PCIE_OK;

    int bar_index = sim_bar_index(reg);

    if (bar_index >= 0) {
        struct lab_pcie_sim_bar *bar = &fn->bars[bar_index];
       
	if (bar->implemented == 0U)
	    return LAB_PCIE_OK;

	if (value == 0xFFFFFFFFU) {
	    bar->probing = 1U;

	    return LAB_PCIE_OK;
	}

	bar->probing = 0U;

	bar->value = (value & ~0xFU) | PCIE_CONF_BAR_FLAGS(bar->value);

	return LAB_PCIE_OK;
    }

    fn->config[reg] = value;

    return LAB_PCIE_OK;
}

const struct lab_pcie_host_ops g_lab_pcie_sim_host_ops = {
    .config_read32 = sim_config_read32,
    .config_write32 = sim_config_write32
};

void lab_pcie_sim_controller_init(struct lab_pcie_sim_controller *controller) {
    struct lab_pcie_sim_function *fn;

    if (controller == NULL)
	return;

    controller->function_count = 1U;

    fn = &controller->functions[0];

    fn->bdf = PCIE_BDF(0U, 1U, 0U);

    fn->present = 1U;

    for (size_t index = 0; index < LAB_PCIE_SIM_CONFIG_DWORDS; ++index) {
        fn->config[index] = 0U;
    }

    for (size_t index = 0U; index < LAB_PCIE_SIM_BAR_COUNT; ++index) {
        fn->bars[index].value = 0U;

	fn->bars[index].size = 0U;

	fn->bars[index].implemented = 0U;

	fn->bars[index].probing = 0U;
    }

    fn->config[PCIE_CONF_ID] = PCIE_ID(0x1234U, 0x5678U);

    fn->config[PCIE_CONF_CLASSREV] = 0x02000001U;

    fn->config[PCIE_CONF_TYPE] = 0x00000000U;

    fn->bars[0].value = 0x00000000U;
    fn->bars[0].size = 0x00001000U;
    fn->bars[0].implemented = 1U;
}

