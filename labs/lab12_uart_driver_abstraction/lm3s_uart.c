#include "device.h"
#include "lm3s_uart.h"


void uart_putc(char c) {
    while (UART0->FR & (1 << 5)) {
        // TX FIFO full
    }
    UART0->DR = c;
}

int uart_getc(void) {
    if (UART0->FR & (1 << 4)) {
        return -1;
    }

    return UART0->DR;
}
