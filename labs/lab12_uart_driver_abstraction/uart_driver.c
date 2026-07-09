#include "uart_driver.h"
#include "uart.h"

static const struct uart_driver_ops uart_ops = {
    .init = uart_init,
    .putc = uart_putc,
    .getc = uart_getc,
};

// .data section
struct uart_device uart0 = {
    .ops = &uart_ops
};
