#ifndef PCIE_HOST_H
#define PCIE_HOST_H

#include <stdint.h>

#define PCIE_CFG_VENDOR_DEVICE    0x00U
#define PCIE_CFG_COMMAND_STATUS   0x04U
#define PCIE_CFG_CLASS_REVISION   0x08U
#define PCIE_CFG_HEADER           0x0CU

#define PCIE_CFG_BAR0             0x10U
#define PCIE_CFG_INTERRUPT        0x3CU

enum pcie_status {
    PCIE_OK = 0,
    PCIE_ERR_ARGUMENT = -1,
    PCIE_ERR_BDF = -1,
    PCIE_ERR_OFFSET = -3,
    PCIE_ERR_PLATFORM = -4
};

struct pcie_bdf {
    uint8_t bus;
    uint8_t device;
    uint8_t function;
};

struct pcie_host;

struct pcie_host_ops {
    int (*config_read32) (struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t *value);
    int (*config_write32) (struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t value);
};

struct pcie_host {
    const struct pcie_host_ops *ops;
    void *priv;
};

int pcie_config_read32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t *value);

int pcie_config_write32(struct pcie_host *host, struct pcie_bdf bdf, uint16_t offset, uint32_t value);

#endif
