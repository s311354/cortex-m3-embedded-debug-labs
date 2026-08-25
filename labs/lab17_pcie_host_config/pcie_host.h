#ifndef PCIE_HOST_H
#define PCIE_HOST_H

#include <stddef.h>
#include <stdint.h>

#define PCIE_VENDOR_DEVICE_ID_OFFSET 0x00U
#define PCIE_COMMAND_STATUS_OFFSET   0x04U
#define PCIE_CLASS_REVISION_OFFSET   0x08U
#define PCIE_HEADER_TYPE_OFFSET      0x0CU

#define PCIE_BAR0_OFFSET             0x10U
#define PCIE_BAR1_OFFSET             0x14U
#define PCIE_BAR2_OFFSET             0x18U
#define PCIE_BAR3_OFFSET             0x1CU
#define PCIE_BAR4_OFFSET             0x20U
#define PCIE_BAR5_OFFSET             0x24U

#define PCIC_VENDOR_ID_INVALID       0xFFFFU

enum pcie_status {
    PCIE_OK = 0,
    PCIE_ERR_ARGUMENT = -1,
    PCIE_ERR_BDF = -2,
    PCIE_ERR_OFFSET = -3,
    PCIE_ERR_NO_DEVICE = -4
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
