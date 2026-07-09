#include <stdint.h>

#include "uart_driver.h"

extern struct uart_device uart0;

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uart0.ops->init();

    // copy .data from FLASH to RAM
    uart0.ops->putc('A');

    while (1) {
        int c;

	c = uart0.ops->getc();

	if (c >= 0) {
	    uart0.ops->putc(c);
	}
    }
}
