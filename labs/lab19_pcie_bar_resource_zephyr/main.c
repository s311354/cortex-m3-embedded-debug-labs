#include <stdint.h>

#include <zephyr/drivers/pcie/pcie.h>

#include "board_pcie.h"
#include "pcie_resource.h"

volatile uint32_t g_stage;

volatile int g_board_result;
volatile int g_probe_result;
volatile int g_assign_result;
volatile int g_enable_result;

volatile uint32_t g_id;

volatile uint16_t g_vendor;
volatile uint16_t g_device;

volatile uint32_t g_bar_original;
volatile uint32_t g_bar_size;

volatile uint32_t g_bar_bus_addr;
volatile uint32_t g_bar_cpu_addr;

volatile uint32_t g_bar_programmed;
volatile uint32_t g_command_status;


struct lab_pcie_bar_resource g_bar0;

__attribute__((noinline))
void lab19_debug_checkpoint(void) {
    __asm volatile ("nop");
}

int main(void) {
    pcie_bdf_t endpoint;

    endpoint = PCIE_BDF(0U, 1U, 0U);

    /* Stage 1*/
    g_stage = 1U;

    g_board_result = board_pcie_init();

    lab19_debug_checkpoint();

    if (g_board_result != LAB_PCIE_OK)
	goto failed;

    /* Endpoint ID */
    (void) lab_pcie_config_read32(&g_board_pcie_host, endpoint, PCIE_CONF_ID, (uint32_t *) &g_id);

    g_vendor = (uint16_t) PCIE_ID_TO_VEND(g_id);

    g_device = (uint16_t) PCIE_ID_TO_DEV(g_id);

    /* Stage 2. Probe BAR0 size */
    g_probe_result = lab_pcie_probe_bar32(&g_board_pcie_host, endpoint, 0U, &g_bar0);

    if (g_probe_result != LAB_PCIE_OK)
	goto failed;


    g_bar_original = g_bar0.original;

    g_bar_size = (uint32_t) g_bar0.size;

    g_stage = 2U;

    lab19_debug_checkpoint();

    /* Stage 3. Allocate PCI resource and program BAR */
    g_assign_result = lab_pcie_assign_bar32(&g_board_pcie_host, endpoint, &g_board_pcie_mem_window, &g_bar0);

    if (g_assign_result != LAB_PCIE_OK)
	goto failed;

    g_bar_bus_addr = (uint32_t) g_bar0.bus_addr;

    g_bar_cpu_addr = (uint32_t) g_bar0.cpu_addr;

    (void) lab_pcie_config_read32(&g_board_pcie_host, endpoint, PCIE_CONF_BAR0, (uint32_t *) &g_bar_programmed);

    g_stage = 3U;

    lab19_debug_checkpoint();

    /* Stage 4 */
    g_enable_result = lab_pcie_enable_memory_space(&g_board_pcie_host, endpoint);

    if (g_enable_result != LAB_PCIE_OK)
	goto failed;

    (void) lab_pcie_config_read32(&g_board_pcie_host, endpoint, PCIE_CONF_CMDSTAT, (uint32_t *) &g_command_status);

    g_stage = 4U;

    lab19_debug_checkpoint();

    while (1) {
    }

failed:
    g_stage = 0xFFFFFFFFU;

    while (1) {
    }
}
