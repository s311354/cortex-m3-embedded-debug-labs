#include "console.h"
#include "uart_driver.h"

extern struct uart_device uart0;

void console_init(void) {
    uart_driver_init(&uart0);
}

void console_putc(char c) {
    uart_driver_putc(&uart0, c);
}

int console_getc(void) {
    return uart_driver_getc(&uart0);
}
