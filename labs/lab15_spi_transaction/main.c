#include <stdint.h>
#include "spi_bitbang.h"
#include "simulated_spi_device.h"
#include "device.h"

#define SPI_COMMAND_READ_ID 0x9FU

enum lab15_stage {
    LAB15_STAGE_RESET = 0,
    LAB15_STAGE_INIT,
    LAB15_STAGE_TRANSFER,
    LAB15_STAGE_VERIFY,
    LAB15_STAGE_DONE,
    LAB15_STAGE_ERROR
};

volatile enum lab15_stage g_lab15_stage;

volatile int g_spi_init_result;
volatile int g_spi_transfer_result;
volatile int g_spi_verify_result;

volatile uint8_t g_spi_tx[4];
volatile uint8_t g_spi_rx[4];

volatile uint32_t g_spi_clock_edges;
volatile uint32_t g_spi_delay_calls;

static struct simulated_spi_device g_simulated_device;
static struct spi_bitbang_bus g_spi_bus;

__attribute__((noinline))
void lab15_debug_checkpoint(void) {
    __asm volatile ("nop");
}

__attribute__((naked))
void SVC_Handler(void) {
    __asm volatile ("bx lr");
}

static void configure_spi_bus(void) {
    g_spi_bus.ops = simulated_spi_device_get_ops();
    g_spi_bus.context = &g_simulated_device;
    g_spi_bus.mode = SPI_MODE_0;
    g_spi_bus.bit_order = SPI_MSB_FIRST;
    g_spi_bus.cs_active_low = 1U;
}

int main(void) {
    uint8_t tx_buffer[4];
    uint8_t rx_buffer[4];

    g_lab15_stage = LAB15_STAGE_RESET;

    simulated_spi_device_init(&g_simulated_device);
    configure_spi_bus();

    tx_buffer[0] = SPI_COMMAND_READ_ID;
    tx_buffer[1] = 0xFFU;
    tx_buffer[2] = 0xFFU;
    tx_buffer[3] = 0xFFU;

    rx_buffer[0] = 0U;
    rx_buffer[1] = 0U;
    rx_buffer[2] = 0U;
    rx_buffer[3] = 0U;

    g_spi_tx[0] = tx_buffer[0];
    g_spi_tx[1] = tx_buffer[1];
    g_spi_tx[2] = tx_buffer[2];
    g_spi_tx[3] = tx_buffer[3];

    g_lab15_stage = LAB15_STAGE_INIT;
    lab15_debug_checkpoint();

    g_spi_init_result = spi_bitbang_init(&g_spi_bus);

    if (g_spi_init_result != SPI_OK) {
        g_lab15_stage = LAB15_STAGE_ERROR;
	lab15_debug_checkpoint();

	while (1) {
	}
    }

    g_lab15_stage = LAB15_STAGE_TRANSFER;
    lab15_debug_checkpoint();

    g_spi_transfer_result = spi_bitbang_transfer(
		            &g_spi_bus,
			    tx_buffer,
			    rx_buffer,
			    4U);

    g_spi_rx[0] = rx_buffer[0];
    g_spi_rx[1] = rx_buffer[1];
    g_spi_rx[2] = rx_buffer[2];
    g_spi_rx[3] = rx_buffer[3];

    g_spi_clock_edges = g_simulated_device.clock_edge_count;

    g_spi_delay_calls = g_simulated_device.delay_count;

    g_lab15_stage = LAB15_STAGE_VERIFY;

    if (g_spi_transfer_result == SPI_OK &&
	rx_buffer[0] == 0xFFU &&
	rx_buffer[1] == 0xEFU &&
	rx_buffer[2] == 0x40U &&
	rx_buffer[3] == 0x18U) {
        g_spi_verify_result = 0;
	g_lab15_stage = LAB15_STAGE_DONE;
    } else {
        g_spi_verify_result = -1;
	g_lab15_stage = LAB15_STAGE_ERROR;
    }

    lab15_debug_checkpoint();

    while (1) {
    }
}
