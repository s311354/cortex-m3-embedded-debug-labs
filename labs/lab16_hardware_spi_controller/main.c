#include <stdint.h>

#include "board_ssp.h"

#define TRANSFER_SIZE 4

volatile uint32_t g_stage;

volatile uint8_t tx_buffer[TRANSFER_SIZE];
volatile uint8_t rx_buffer[TRANSFER_SIZE];

volatile int init_result;
volatile int transfer_result;
volatile int verify_result;

volatile uint32_t reg_cr0;
volatile uint32_t reg_cr1;
volatile uint32_t reg_sr;
volatile uint32_t reg_cpsr;
volatile uint32_t reg_imsc;

__attribute__((noinline)) 
void debug_checkpoint(void) {
    __asm volatile("nop");
}

static void capture_register(void) {
    reg_cr0 = g_board_ssp3.regs->CR0;

    reg_cr1 = g_board_ssp3.regs->CR1;

    reg_sr = g_board_ssp3.regs->SR;

    reg_cpsr = g_board_ssp3.regs->CPSR;

    reg_imsc = g_board_ssp3.regs->IMSC;
}

int main (void) {
    tx_buffer[0] = 0x9f;
    tx_buffer[1] = 0xa5;
    tx_buffer[2] = 0x5a;
    tx_buffer[3] = 0xff;

    for (uint32_t i = 0; i < TRANSFER_SIZE; ++i) {
        rx_buffer[i] = 0;
    }


    g_stage = 1;

    debug_checkpoint();

    init_result = mps2_ssp_init(&g_board_ssp3, &g_board_ssp3_config);

    capture_register();

    g_stage = 2;

    debug_checkpoint();

    transfer_result = mps2_ssp_transfer(&g_board_ssp3, tx_buffer, rx_buffer, TRANSFER_SIZE);

    capture_register();

    verify_result = 0;

    for (uint32_t i = 0; i < TRANSFER_SIZE; ++i) {
        if (tx_buffer[i] != rx_buffer[i])
	    verify_result = -1;
    }

    g_stage = 3;

    debug_checkpoint();

    // interrupt mask test

    mps2_ssp_enable_interrupts(&g_board_ssp3, SSP_IMSC_RXIM_Msk | SSP_IMSC_RTIM_Msk);

    capture_register();

    debug_checkpoint();

    mps2_ssp_disable_interrupts(&g_board_ssp3, SSP_IMSC_RXIM_Msk | SSP_IMSC_RTIM_Msk);

    g_stage = 4;

    debug_checkpoint();

    while (1) {
    }

}
