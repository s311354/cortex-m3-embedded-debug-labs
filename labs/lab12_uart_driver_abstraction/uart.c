#include "uart.h"

void uart_init(void) {
    /* 1. Disable UART */
    UART0->CTRL = 0x0;

    /* 2. Set baud rate (example: 115200 for 25MHz clock) */
    UART0->BAUDDIV = 217;

    /* 3. Enable UART, TX, RX */
    UART0->CTRL = CM3DS_MPS2_UART_CTRL_TXEN_Msk | CM3DS_MPS2_UART_CTRL_RXEN_Msk | CM3DS_MPS2_UART_CTRL_RXIRQEN_Msk;
}

void uart_putc(char c) {
    while (UART0->STATE & CM3DS_MPS2_UART_STATE_TXBF_Msk) {
    }

    UART0->DATA = (uint32_t) c;
}

char uart_getc(void) {
    while (!(UART0->STATE & CM3DS_MPS2_UART_STATE_RXBF_Msk)) {
    }

    return (char) (UART0->DATA & CM3DS_MPS2_UART_DATA_Msk);
}
