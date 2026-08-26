#include <stdint.h>

#include "board_pcie.h"
#include "pcie_bus.h"

volatile uint32_t g_stage;

volatile int g_board_result;
volatile int g_bus_result;
volatile int g_enum_result;

volatile uint32_t g_device_count;

volatile uint16_t g_first_vendor;
volatile uint16_t g_first_device;

volatile uint32_t g_first_bar0;

volatile uint8_t g_first_irq_line;
volatile uint8_t g_first_irq_pin;

struct pcie_bus g_pcie_bus;

__attribute__((noinline))
void lab18_debug_checkpoint(void) {
    __asm volatile ("nop");
}

int main(void) {
    struct pcie_device *dev;

    g_stage = 1U;

    g_board_result = board_pcie_init();

    lab18_debug_checkpoint();

    if (g_board_result != PCIE_OK)
	goto failed;

    g_bus_result = pcie_bus_init(&g_pcie_bus, &g_board_pcie_host);

    if (g_bus_result != PCIE_OK)
	goto failed;

    g_stage = 2U;

    lab18_debug_checkpoint();

    g_enum_result = pcie_enumerate(&g_pcie_bus);

    if (g_enum_result != PCIE_OK)
	goto failed;

    g_device_count = (uint32_t) g_pcie_bus.device_count;

    g_stage = 3U;

    lab18_debug_checkpoint();

    if (g_pcie_bus.device_count > 0U) {
        dev = &g_pcie_bus.devices[0];

	g_first_vendor = dev->vendor_id;

	g_first_device = dev->device_id;

	g_first_bar0 = dev->bars[0].address;

	g_first_irq_line = dev->irq_line;

	g_first_irq_pin = dev->irq_pin;
    }

    g_stage = 4U;

    lab18_debug_checkpoint();

    while (1) {
    }

failed:
    g_stage = 0xFFFFFFFFU;

    lab18_debug_checkpoint();

    while (1) {
    }
}
