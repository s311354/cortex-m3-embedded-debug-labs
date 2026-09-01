#include <stdint.h>

#include "labh1_pcie_regs.h"
#include "pcie_ahb_host.h"

volatile uint32_t g_stage;

volatile int g_init_result;
volatile int g_cfg_result;

volatile uint32_t g_version;
volatile uint32_t g_endpoint_bdf;
volatile uint32_t g_id;

volatile uint16_t g_vendor;
volatile uint16_t g_device;

void labh1_debug_checkpoint(void);

__attribute__((noinline))
void labh1_debug_checkpoint(void) {
    __asm volatile ("nop");
}

int main(void) {
    g_stage = 1U;

    g_version = labh1_pcie_version();

    g_init_result = labh1_pcie_host_init();

    labh1_debug_checkpoint();

    if (g_init_result != LABH1_OK)
	goto failed;

    g_endpoint_bdf = LABH1_PCIE_BDF(0U, 1U, 0U);

    g_stage = 2U;

    g_cfg_result = labh1_pcie_config_read32(g_endpoint_bdf, 0U, (uint32_t*) &g_id);

    labh1_debug_checkpoint();

    if (g_cfg_result != LABH1_OK)
	goto failed;

    g_vendor = (uint16_t)(g_id & 0xFFFFU);
    g_device = (uint16_t)(g_id >> 16U);

    if ((g_vendor != 0x1234U) || (g_device != 0x5678U))
	goto failed;

    g_stage = 3U;

    labh1_debug_checkpoint();

    while (1) {
    }

failed:
    g_stage = 0xFFFFFFFFU;

    labh1_debug_checkpoint();

    while (1) {
    }
}
