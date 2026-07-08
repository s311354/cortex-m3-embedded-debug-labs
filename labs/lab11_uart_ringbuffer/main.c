#include <stdint.h>

#include "uart.h"
#include "ringbuffer.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    rb_init();

    uart_init();

    uart_enable_irq();

    uart_puts("Lab 11 Ring Buffer\r\n");

    while (1) {
        if (!rb_empty())
	    uart_putc(rb_pop());
    }
}
