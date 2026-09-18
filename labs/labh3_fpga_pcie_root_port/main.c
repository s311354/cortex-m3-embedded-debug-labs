#include <stdint.h>

#include "device.h"

#include "m3ds_pcie_host_regs.h"
#include "pci_config.h"
#include "labh3_endpoint.h"

#define LABH3_POLL_LIMIT     1000000U

enum labh3_result {
    LABH3_OK              = 0,

    LABH3_ERR_BUSY        = -1,
    LABH3_ERR_BACKEND     = -2,
    LABH3_ERR_TIMEOUT     = -3,

    LABH3_ERR_NO_DEVICE   = -4,
    LABH3_ERR_BAR_TYPE    = -5,
    LABH3_ERR_BAR_SIZE    = -6,
    LABH3_ERR_BAR_PROGRAM = -7,

    LABH3_ERR_MMIO        = -8
};

static volatile struct m3ds_pcie_host_regs *const g_pcie = (volatile struct m3ds_pcie_host_regs *) M3DS_PCIE_HOST_BASE;

static volatile uint32_t *const g_bar0_scratch = (volatile uint32_t *) (M3DS_PCIE_MMIO_BASE + LABH3_BAR0_SCRATCH_OFFSET);


/* Debug-visible state */
volatile uint32_t g_stage;

volatile int g_result;

volatile uint32_t g_version;
volatile uint32_t g_id;

volatile uint16_t g_vendor;
volatile uint16_t g_device;

volatile uint32_t g_bar0_original;
volatile uint32_t g_bar0_mask;
volatile uint32_t g_bar0_size;
volatile uint32_t g_bar0_readback;

volatile uint32_t g_command;

volatile uint32_t g_mmio_readback;

__attribute__((noinline))
static void labh3_debug_checkpoint(void) {
    __asm volatile ("nop");
}

static int wait_completion(uint32_t *read_data) {
    for (uint32_t timeout = 0U; timeout < LABH3_POLL_LIMIT; ++timeout) {
        uint32_t status = g_pcie->status;

	if ((status & M3DS_PCIE_STATUS_DONE) == 0U)
	    continue;

	if ((status & M3DS_PCIE_STATUS_ERROR) != 0U)
	    return LABH3_ERR_BACKEND;

	if (read_data != 0)
	    *read_data = g_pcie->cfg_rdata;

	return LABH3_OK;
    }

    return LABH3_ERR_TIMEOUT;
}

static int config_read32(uint32_t bdf, uint32_t reg, uint32_t *value) {
    if ((g_pcie->status & M3DS_PCIE_STATUS_BUSY) != 0U)
	return LABH3_ERR_BUSY;

    g_pcie->cfg_bdf = bdf;
    g_pcie->cfg_reg = reg;

    g_pcie->cfg_command = M3DS_PCIE_CMD_CFG_READ;

    return wait_completion(value);
}

static int config_write32(uint32_t bdf, uint32_t reg, uint32_t value) {
    if ((g_pcie->status & M3DS_PCIE_STATUS_BUSY) != 0U)
	return LABH3_ERR_BUSY;

    g_pcie->cfg_bdf = bdf;
    g_pcie->cfg_reg = reg;

    g_pcie->cfg_command = M3DS_PCIE_CMD_CFG_WRITE;

    g_pcie->cfg_wdata = value;

    return wait_completion(0);
}

int main(void) {
    const uint32_t endpoint_bdf = M3DS_PCIE_BDF(LABH3_ENDPOINT_BUS, LABH3_ENDPOINT_DEV, LABH3_ENDPOINT_FN);

    /* Stage 1. Host Controller compatibility */
    g_stage = 1U;

    g_version = g_pcie->version;

    if (g_version != M3DS_PCIE_VERSION_VALUE)
	goto failed;

    g_pcie->control = M3DS_PCIE_CONTROL_ENABLE;

    labh3_debug_checkpoint();

    /* Stage 2. Real PCIe Configuration Read */
    g_stage = 2U;

    g_result = config_read32(endpoint_bdf, PCI_CFG_VENDOR_DEVICE, (uint32_t *)&g_id);

    if (g_result != LABH3_OK)
	goto failed;

    g_vendor = (uint16_t) (g_id & 0xFFFFU);
    g_device = (uint16_t) ((g_id >> 16U) & 0xFFFFU);

    if ((g_vendor == 0xFFFFU) || (g_vendor == 0x0000U)) {
        g_result = LABH3_ERR_NO_DEVICE;

	goto failed;
    }

    labh3_debug_checkpoint();

    /* Stage 3. Probe BAR0 */
    g_stage = 3U;

    g_result = config_read32(endpoint_bdf, PCI_CFG_BAR0, (uint32_t *)&g_bar0_original);

    if (g_result != LABH3_OK)
	goto failed;

    if ((g_bar0_original & PCI_BAR_IO_SPACE) != 0U) {
        g_result = LABH3_ERR_BAR_TYPE;
	goto failed;
    }

    if ((g_bar0_original & PCI_BAR_MEM_TYPE_MASK) != PCI_BAR_MEM_TYPE_32) {
        g_result = LABH3_ERR_BAR_TYPE;
	goto failed;
    }

    g_result = config_write32(endpoint_bdf, PCI_CFG_BAR0, 0xFFFFFFFFU);

    if (g_result != LABH3_OK)
	goto failed;

    g_result = config_read32(endpoint_bdf, PCI_CFG_BAR0, (uint32_t *)&g_bar0_mask);

    if (g_result != LABH3_OK)
	goto failed;

    g_bar0_size = ~(g_bar0_mask & PCI_BAR_MEM_ADDR_MASK) + 1U;

    if ((g_bar0_size == 0U) || (g_bar0_size > M3DS_PCIE_MMIO_SIZE) || (LABH3_BAR0_SCRATCH_OFFSET >= g_bar0_size)) {
        g_result = LABH3_ERR_BAR_SIZE;

	goto failed;
    }

    /*
     * Aperture base must satisfy BAR alignment
     */
    if ((M3DS_PCIE_MMIO_BASE & (g_bar0_size - 1U)) != 0U) {
        g_result = LABH3_ERR_BAR_SIZE;

	goto failed;
    }

    /* porgram BAR0 into the CPU-visible PCIe aperture */
    g_result = config_write32(endpoint_bdf, PCI_CFG_BAR0, M3DS_PCIE_MMIO_BASE);

    if (g_result != LABH3_OK)
	goto failed;

    g_result = config_read32(endpoint_bdf, PCI_CFG_BAR0, (uint32_t *) &g_bar0_readback);

    if (g_result != LABH3_OK)
	goto failed;

    if ((g_bar0_readback & PCI_BAR_MEM_ADDR_MASK) != M3DS_PCIE_MMIO_BASE ) {
        g_result = LABH3_ERR_BAR_PROGRAM;

	goto failed;
    }

    labh3_debug_checkpoint();

    /* Stage 4. Enable PCI Memory Space */
    g_stage = 4U;

    uint32_t command_status;

    g_result = config_read32(endpoint_bdf, PCI_CFG_COMMAND_STATUS, &command_status);

    if (g_result != LABH3_OK)
	goto failed;

    /*
     * Preserve Command[15:0]
     */
    g_command = (command_status & 0x0000FFFFU) | PCI_COMMAND_MEMORY_ENABLE;

    g_result = config_write32(endpoint_bdf, PCI_CFG_COMMAND_STATUS, g_command);

    if (g_result != LABH3_OK)
	goto failed;

    labh3_debug_checkpoint();

    /* Stage 5. PCIe Memory Write */

    g_stage = 5U;

    *g_bar0_scratch = LABH3_TEST_VALUE;

    __DSB();

    labh3_debug_checkpoint();

    /* Stage 6. PCIe Memory Read */

    g_stage = 6U;

    g_mmio_readback = *g_bar0_scratch;

    if (g_mmio_readback != LABH3_TEST_VALUE) {
        g_result = LABH3_ERR_MMIO;

	goto failed;
    }

    /*
     * PASS
     */

    g_result = LABH3_OK;

    g_stage = 7U;

    labh3_debug_checkpoint();

    while (1) {
    }


failed:
    g_stage  = 0xFFFFFFFFU;
    labh3_debug_checkpoint();

    while (1) {
    }

}


