#include "uart.h"

#define UART_CTRL_TX_EN    CM3DS_MPS2_UART_CTRL_TXEN_Msk
#define UART_CTRL_RX_EN    CM3DS_MPS2_UART_CTRL_RXEN_Msk

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
     */
    UART0->CTRL = UART_CTRL_TX_EN | UART_CTRL_RX_EN;
}
