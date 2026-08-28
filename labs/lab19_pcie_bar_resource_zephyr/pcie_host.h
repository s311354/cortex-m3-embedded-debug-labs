#ifndef LAB19_PCIE_HOST_H
#define LAB19_PCIE_HOST_H

#include <stdint.h>

#include <zephyr/drivers/pcie/pcie.h>

enum lab_pcie_status {
    LAB_PCIE_OK = 0,
    LAB_PCIE_ERR_ARGUMENT = -1,
    LAB_PCIE_ERR_BDF = -2,
    LAB_PCIE_ERR_REG = -3,
    LAB_PCIE_ERR_PLATFORM = -4,
    LAB_PCIE_ERR_NO_RESOURCE = -5,
    LAB_PCIE_ERR_UNSUPPORTED = -6
};

struct lab_pcie_host;

struct lab_pcie_host_ops {
    int (*config_read32) (struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t *value);
    int (*config_write32) (struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t value);
};

struct lab_pcie_host {
    const struct lab_pcie_host_ops *ops;
    void *priv;
};

int lab_pcie_config_read32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t *value);

int lab_pcie_config_write32(struct lab_pcie_host *host, pcie_bdf_t bdf, unsigned int reg, uint32_t value);

#endif
