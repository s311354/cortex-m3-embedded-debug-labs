#ifndef LABH1_PCIE_AHB_HOST_H
#define LABH1_PCIE_AHB_HOST_H

#include <stdint.h>

enum labh1_status {
    LABH1_OK = 0,
    LABH1_ERR_ARGUMENT = -1,
    LABH1_ERR_VERSION = -2,
    LABH1_ERR_BUSY = -3,
    LABH1_ERR_TIMEOUT = -4,
    LABH1_ERR_BACKEND = -5
};

uint32_t labh1_pcie_version(void);

int labh1_pcie_host_init(void);

int labh1_pcie_config_read32(uint32_t bdf, unsigned int reg, uint32_t *value);

int labh1_pcie_config_write32(uint32_t bdf, unsigned reg, uint32_t value);

#endif
