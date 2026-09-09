#include <stdint.h>

#include "m3ds_pcie_host_regs.h"

#define LABH2_POLL_LIMIT           100000U

/*
 * LabH2 educational writable configuration register.
 */
#define LABH2_CFG_SCRATCH_OFFSET    0x040U
#define LABH2_CFG_SCRATCH_VALUE     0xA5A55A5AU

enum labh2_result {
    LABH2_OK = 0,
    LABH2_ERR_BUSY = -1,
    LABH2_ERR_BACKEND = -2,
    LABH2_ERR_TIMEOUT = -3
};

static volatile struct m3ds_pcie_host_regs *const g_pcie = (volatile struct m3ds_pcie_host_regs *) M3DS_PCIE_HOST_BASE;

volatile uint32_t g_stage;
volatile uint32_t g_status;

volatile uint32_t g_version;
volatile uint32_t g_id;
volatile uint32_t g_readback;
volatile uint32_t g_absent_value;

volatile uint16_t g_vendor;
volatile uint16_t g_device;

volatile int g_result;

__attribute__((noinline))
static void labh2_debug_checkpoint(void) {
    __asm volatile ("nop");
}

static int wait_completion(uint32_t *read_data) {
    for (uint32_t timeout = 0U; timeout < LABH2_POLL_LIMIT; ++timeout) {
        uint32_t status = g_pcie->status;

	g_status = status;

	if ((status & M3DS_PCIE_STATUS_DONE) == 0U)
            continue;

	if ((status & M3DS_PCIE_STATUS_ERROR) != 0U)
	    return LABH2_ERR_BACKEND;

	if (read_data != 0)
	    *read_data = g_pcie->cfg_rdata;
    
        return LABH2_OK;
    }

    return LABH2_ERR_TIMEOUT;
}

static int config_read32(uint32_t bdf, uint32_t reg, uint32_t *value) {
    if ((g_pcie->status & M3DS_PCIE_STATUS_BUSY) != 0U)
	return LABH2_ERR_BUSY;

    g_pcie->cfg_bdf = bdf;
    g_pcie->cfg_reg = reg;

    g_pcie->cfg_command = M3DS_PCIE_CMD_CFG_READ;

    return wait_completion(value);
}

static int config_write32(uint32_t bdf, uint32_t reg, uint32_t value) {
    if ((g_pcie->status & M3DS_PCIE_STATUS_BUSY) != 0U)
	return LABH2_ERR_BUSY;

    g_pcie->cfg_bdf = bdf;
    g_pcie->cfg_reg = reg;
    g_pcie->cfg_wdata = value;

    g_pcie->cfg_command = M3DS_PCIE_CMD_CFG_WRITE;

    return wait_completion(0);
}

int main(void) {
    const uint32_t endpoint_bdf = M3DS_PCIE_BDF(0U, 1U, 0U);

    const uint32_t absent_bdf = M3DS_PCIE_BDF(0U, 2U, 0U);

    g_stage = 1U;

    g_version = g_pcie->version;

    if (g_version != M3DS_PCIE_VERSION_VALUE)
	goto failed;

    g_pcie->control = M3DS_PCIE_CONTROL_ENABLE;

    /*
     * Stage 2.
     * Config Read - 00:01.0 DWORD 0
     */
    g_stage = 2U;

    g_result = config_read32(endpoint_bdf, 0x000U, (uint32_t *)&g_id);

    labh2_debug_checkpoint();

    if (g_result != LABH2_OK)
	goto failed;

    g_vendor = (uint16_t)(g_id & 0xFFFFU);
    g_device = (uint16_t)(g_id >> 16U);

    if ((g_vendor != 0x1234U) || (g_device != 0x5678U))
	goto failed;

    /*
     * Stage 3.
     * H2-specific CFG_WRITE
     */
    g_stage = 3U;

    g_result = config_write32(endpoint_bdf, LABH2_CFG_SCRATCH_OFFSET, LABH2_CFG_SCRATCH_VALUE);

    if (g_result != LABH2_OK)
	goto failed;

    /*
     * Stage 4:
     * Read back written config value
     */
    g_stage = 4U;

    g_result = config_read32(endpoint_bdf, LABH2_CFG_SCRATCH_OFFSET, (uint32_t *)&g_readback);

    labh2_debug_checkpoint();

    if ((g_result != LABH2_OK) || (g_readback != LABH2_CFG_SCRATCH_VALUE)) 
	goto failed;

    /*
     * Stage 4:
     * Absent endpoint must preserve H1 enumeration contract
     */
    g_stage = 5U;

    g_result = config_read32(absent_bdf, 0x000U, (uint32_t *)&g_absent_value);

    labh2_debug_checkpoint();

    if ((g_result != LABH2_OK) || (g_absent_value != 0xFFFFFFFFU))
	goto failed;

    g_stage = 6U;

    labh2_debug_checkpoint();

    while (1) {
    }

failed:
    g_stage = 0xFFFFFFFFU;

    labh2_debug_checkpoint();

    while (1) {
    }
}
