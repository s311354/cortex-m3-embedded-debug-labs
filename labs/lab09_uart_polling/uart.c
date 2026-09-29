#include "uart.h"

#define UART_CTRL_TX_EN    CM3DS_MPS2_UART_CTRL_TXEN_Msk
#define UART_CTRL_RX_EN    CM3DS_MPS2_UART_CTRL_RXEN_Msk

#define UART_STATE_TX_FULL CM3DS_MPS2_UART_STATE_TXBF_Msk
#define UART_STATE_RX_FULL CM3DS_MPS2_UART_STATE_RXBF_Msk

void uart_init(void) {
    /* 1. Disable UART */
    UART0->CTRL = 0x0;

    /* 2. Set baud rate (example: 115200 for 25MHz clock) */
    UART0->BAUDDIV = 217;

    /* 3. Enable UART, TX, RX */
    UART0->CTRL = UART_CTRL_TX_EN | UART_CTRL_RX_EN;
}

void uart_putc(char c) {
    while (UART0->STATE & UART_STATE_TX_FULL) {
        /* Busy-wait until the TX buffer can accept data */
    }

    UART0->DATA = (uint32_t) c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

char uart_getc(void) {
    while (!(UART0->STATE & UART_STATE_RX_FULL)) {
	/* Busy-wait until the RX buffer contains data */
    }

    return (char) (UART0->DATA & CM3DS_MPS2_UART_DATA_Msk);
}
