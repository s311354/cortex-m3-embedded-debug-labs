#include <stdint.h>
#include "uart.h"
#include "device.h"
#include "ringbuffer.h"

__attribute__((naked))
void SVC_Handler(void) {

}

static ringbuffer_t rx_rb;
static volatile bool rx_overflow;

void UART0_Handler(void) {
    char c = UART0->DATA & CM3DS_MPS2_UART_DATA_Msk;

    if (!rb_push(&rx_rb, c))
	rx_overflow = true;


    UART0->INTCLEAR = CM3DS_MPS2_UART_CTRL_RXIRQ_Msk;
}

int main(void) {
    uint8_t c;

    uart_init();

    rb_init(&rx_rb);

    /* Enable interrupts only after the shared ring buffer is initialized */
    uart_enable_irq();

    while (1) {
        if (rb_pop(&rx_rb, &c))
	    uart_putc(c);
    }
}
