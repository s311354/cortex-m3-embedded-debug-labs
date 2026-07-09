#include <stdint.h>
#include "uart.h"
#include "device.h"
#include "ringbuffer.h"

__attribute__((naked))
void SVC_Handler(void) {

}

static ringbuffer_t rx_rb;

void UART0_Handler(void) {
    char c = UART0->DATA & CM3DS_MPS2_UART_DATA_Msk;

    rb_push(&rx_rb, c);

    UART0->INTCLEAR = CM3DS_MPS2_UART_CTRL_RXIRQ_Msk;
}

int main(void) {
    uint8_t c;

    uart_init();

    rb_init(&rx_rb);

    while (1) {
        if (rb_pop(&rx_rb, &c))
	    uart_putc(c);
    }
}
