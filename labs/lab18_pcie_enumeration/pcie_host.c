#include <stddef.h>

#include "pcie_host.h"

static int pcie_validate_bdf(struct pcie_bdf bdf) {
    if (bdf.device > 31U)
        return PCIE_ERR_BDF;

    if (bdf.function > 7U)
	return PCIE_ERR_BDF;

    return PCIE_OK;
}

static int pcie_validate_offset(uint16_t offset) {
    if (offset >= 256U)
        return PCIE_ERR_OFFSET;

    if ((offset & 0x3U) != 0U)
	return PCIE_ERR_OFFSET;

    return PCIE_OK;
}

int pcie_config_read32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t *value) {
    int result;

    if ((host == NULL) || (host->ops == NULL) || (host->ops->config_read32 == NULL) || (value == NULL))
	return PCIE_ERR_ARGUMENT;

    result = pcie_validate_bdf(bdf);

    if (result != PCIE_OK)
	return result;

    result = pcie_validate_offset(offset);

    if (result != PCIE_OK)
	return result;

    return host->ops->config_read32(host, bdf, offset, value);
}

int pcie_config_write32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t value) {
    int result;

    if ((host == NULL) || (host->ops == NULL) || (host->ops->config_write32 == NULL)) {
        return PCIE_ERR_ARGUMENT;
    }

    result = pcie_validate_bdf(bdf);

    if (result != PCIE_OK)
	return result;

    result = pcie_validate_offset(offset);

    if (result != PCIE_OK)
	return result;

    return host->ops->config_write32(host, bdf, offset, value);
}
