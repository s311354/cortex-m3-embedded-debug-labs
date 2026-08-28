#include <stddef.h>

#include "pcie_host.h"

#define LAB_PCIE_CFG_DWORDS 64U

static int lab_validate_bdf(pcie_bdf_t bdf) {
    if (PCIE_BDF_TO_DEV(bdf) > 31U)
	return LAB_PCIE_ERR_BDF;

    if (PCIE_BDF_TO_FUNC(bdf) > 7U)
	return LAB_PCIE_ERR_BDF;

    return LAB_PCIE_OK;
}

static int lab_validate_reg(unsigned int reg) {
    if (reg >= LAB_PCIE_CFG_DWORDS)
	return LAB_PCIE_ERR_REG;

    return LAB_PCIE_OK;
}

int lab_pcie_config_read32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t *value) {
    int result;

    if ((host == NULL) || (host->ops == NULL) || (host->ops->config_read32 == NULL) || (value == NULL)) {
        return LAB_PCIE_ERR_ARGUMENT;
    }

    result = lab_validate_bdf(bdf);

    if (result != LAB_PCIE_OK)
	return result;

    result = lab_validate_reg(reg);

    if (result != LAB_PCIE_OK)
	return result;

    return host->ops->config_read32(host, bdf, reg, value);
}

int lab_pcie_config_write32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t value) {
    int result;

    if ((host == NULL) || (host->ops == NULL) || (host->ops->config_write32 == NULL))
	return LAB_PCIE_ERR_ARGUMENT;

    result = lab_validate_bdf(bdf);

    if (result != LAB_PCIE_OK)
	return result;

    result = lab_validate_reg(reg);

    if (result != LAB_PCIE_OK)
	return result;

    return host->ops->config_write32(host, bdf, reg, value);
}
