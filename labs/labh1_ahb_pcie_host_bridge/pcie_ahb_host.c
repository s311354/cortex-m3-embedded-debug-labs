#include <stddef.h>
#include <stdint.h>

#include "labh1_pcie_regs.h"
#include "pcie_ahb_host.h"

#define LABH1_POLL_LIMIT 100000U

static volatile struct labh1_pcie_regs *const g_pcie_regs = 
    (volatile struct labh1_pcie_regs *) LABH1_AHB_PCIE_BASE;

uint32_t labh1_pcie_version(void) {
   return g_pcie_regs->version;
}

int labh1_pcie_host_init(void) {
    if (g_pcie_regs->version != LABH1_PCIE_VERSION_VALUE) {
        return LABH1_ERR_VERSION;
    }

    g_pcie_regs->control = LABH1_CONTROL_ENABLE;

    return LABH1_OK;
}

static int labh1_wait_completion(uint32_t *read_data) {
    unsigned int timeout;

    for (timeout = 0U; timeout < LABH1_POLL_LIMIT; ++timeout) {
        uint32_t status = g_pcie_regs->status;

	if ((status & LABH1_STATUS_DONE) == 0U)
	    continue;

	if ((status & LABH1_STATUS_ERROR) != 0U)
	    return LABH1_ERR_BACKEND;

	if (read_data != NULL)
	    *read_data = g_pcie_regs->cfg_rdata;

	return LABH1_OK;
    }

    return LABH1_ERR_TIMEOUT;
}

int labh1_pcie_config_read32(uint32_t bdf, unsigned int reg, uint32_t *value) {
    if (value == NULL)
	return LABH1_ERR_ARGUMENT;

    uint32_t status = g_pcie_regs->status;

    if ((status & LABH1_STATUS_BUSY) != 0U)
	return LABH1_ERR_BUSY;

    g_pcie_regs->cfg_bdf = bdf;
    g_pcie_regs->cfg_reg = reg;

    /*
     * Command write is the "doorbell".
     */
    g_pcie_regs->cfg_command = LABH1_CMD_CFG_READ;

    return labh1_wait_completion(value);
}

int labh1_pcie_config_write32(uint32_t bdf, unsigned int reg, uint32_t value) {

    uint32_t status = g_pcie_regs->status;

    if ((status & LABH1_STATUS_BUSY) != 0U)
	return LABH1_ERR_BUSY;

    g_pcie_regs->cfg_bdf = bdf;
    g_pcie_regs->cfg_reg = reg;
    g_pcie_regs->cfg_wdata = value;

    g_pcie_regs->cfg_command = LABH1_CMD_CFG_WRITE;

    return labh1_wait_completion(NULL);
}

