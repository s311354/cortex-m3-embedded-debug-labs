#include "uart.h"

#define UART_CTRL_TX_EN    (1u << 0)
#define UART_CTRL_RX_EN    (1u << 1)

#define UART_STATE_TX_FULL (1u << 0)
#define UART_STATE_RX_FULL (1u << 0)

void uart_init(void) {
    /* 1. Disable UART */
    UART0->CTRL = 0x0;

    /* 2. Set baud rate (example: 115200 for 25MHz clock) */
    UART0->BAUDDIV = 217;

    /* 3. Enable UART, TX, RX */
    UART0->CTRL = UART_CTRL_TX_EN | UART_CTRL_RX_EN;
}

void uart_putc(char c) {
    while (UART0->STATE & UART_STATE_TX_FULL);
    UART0->DATA = (uint32_t) c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

char uart_getc(void) {
    while (!(UART0->STATE & UART_STATE_RX_FULL)) {
    }

    return (char) UART0->DATA;
}
