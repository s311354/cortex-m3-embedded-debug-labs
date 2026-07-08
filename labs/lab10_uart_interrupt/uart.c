#include "device.h"
#include "uart.h"


void uart_init(void) {
    /* 1. Disable UART */
    UART0->CTL = 0x0;

    /* 2. Set baud rate (example: 115200 for 16MHz clock) */
    UART0->IBRD = 8;    // integer part
    UART0->FBRD = 44;   // fractional part

    /* 3. Line control: 8-bit, no parity, 1 stop bit, FIFO disabled */
    UART0->LCRH = (3 << 5);   // WLEN = 8 bits

    /* 4. Enable UART, TX, RX */
    UART0->CTL = (1 << 0) | (1 << 8) | (1 << 9);
}

#define UART_RXIM (1 << 4)
//#define UART0_IRQn 0

void uart_enable_irq(void) {
    UART0->ICR = 0x7FF;

    UART0->IMSC |= UART_RXIM;

    NVIC_EnableIRQ(UART0_IRQn);
}

void uart_putc(char c) {
    while (UART0->FR & (1 << 5));
    UART0->DR = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

void UART0_Handler(void) {
    if (UART0->MIS & (1 << 4)) {
        char c = UART0->DR;

        uart_putc(c);

	UART0->ICR = (1 << 4);
    }
}
