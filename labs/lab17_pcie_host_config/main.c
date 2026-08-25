#include <stdint.h>

#include "board_pcie.h"
#include "pcie_host.h"

volatile uint32_t g_stage;

volatile int g_init_result;
volatile int g_read_result;

volatile uint32_t g_id_reg;
volatile uint32_t g_command_status;
volatile uint32_t g_class_revision;
volatile uint32_t g_header_type;
volatile uint32_t g_bar0;

volatile uint16_t g_vendor_id;
volatile uint16_t g_device_id;

volatile uint8_t g_class_code;
volatile uint8_t g_subclass;
volatile uint8_t g_prog_if;
volatile uint8_t g_revision;
volatile uint8_t g_header_type_value;

__attribute__((noinline))
void lab17_debug_checkpoint(void) {
    __asm volatile ("nop");
}

int main(void) {
    struct pcie_bdf endpoint = {
        .bus = 0U,
	.device = 1U,
        .function = 0U
    };

    g_stage = 1U;

    g_init_result = board_pcie_init();

    lab17_debug_checkpoint();

    if (g_init_result != PCIE_OK) {
        g_stage = 0xFFFFFFFFU;

	lab17_debug_checkpoint();

	while (1) {
	}
    }

    /* Vendor ID / Device ID */
    g_read_result = pcie_config_read32(&g_board_pcie_host, endpoint, 
		                       PCIE_VENDOR_DEVICE_ID_OFFSET, (uint32_t *) &g_id_reg);

    g_vendor_id = (uint16_t)(g_id_reg & 0xFFFFU);

    g_device_id = (uint16_t)(g_id_reg >> 16U);

    g_stage = 2U;

    lab17_debug_checkpoint();

    /* Command / Status */
    g_read_result = pcie_config_read32(&g_board_pcie_host, endpoint, 
		                       PCIE_COMMAND_STATUS_OFFSET, (uint32_t *) &g_command_status);

    /* Class / Subclass / Prog IF / Revision */
    g_read_result = pcie_config_read32(&g_board_pcie_host, endpoint, 
		                       PCIE_CLASS_REVISION_OFFSET, (uint32_t *) &g_class_revision);

    g_revision = (uint8_t)(g_class_revision & 0xFFU);

    g_prog_if = (uint8_t)((g_class_revision >> 8U) & 0xFFU);

    g_subclass = (uint8_t)((g_class_revision) >> 16U & 0xFFU);

    g_class_code = (uint8_t)((g_class_revision >> 24U) & 0xFFU);

    /* Header Type*/
    g_read_result = pcie_config_read32(&g_board_pcie_host, endpoint,
		                       PCIE_HEADER_TYPE_OFFSET, (uint32_t *) &g_header_type);



    g_header_type_value = (uint8_t)((g_header_type >> 16U) & 0x7FU);

    g_stage = 3U;
    
    lab17_debug_checkpoint();

    /* BAR0 */
    g_read_result = pcie_config_read32(&g_board_pcie_host, endpoint, 
		                       PCIE_BAR0_OFFSET, (uint32_t *) &g_bar0);

    g_stage = 4U;

    lab17_debug_checkpoint();

    while (1) {
    }

}
