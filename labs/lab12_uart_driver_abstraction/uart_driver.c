#include "uart_driver.h"
#include "lm3s_uart.h"

static void lm3s_putc(char c) {
    uart_putc(c);
}

static int lm3s_getc(void) {
    return uart_getc();
}

static const uart_ops_t uart_ops = {
    .putc = lm3s_putc,
    .getc = lm3s_getc,
};

// .data section
uart_device_t uart0 = {
    .ops = &uart_ops,
};
