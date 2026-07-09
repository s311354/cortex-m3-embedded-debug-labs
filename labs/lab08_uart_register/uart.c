#include "uart.h"

void uart_init(void) {
    UART0->CTRL = 0;

    /*
     * UART Clock = 25 MHz
     * BaudDiv = UARTCLK / Baud
     */
    UART0->BAUDDIV = 217;

    /*
     * Enable TX
     * Enable Rx
     * Enable UART 
     */
    UART0->CTRL = 0x03;
}
