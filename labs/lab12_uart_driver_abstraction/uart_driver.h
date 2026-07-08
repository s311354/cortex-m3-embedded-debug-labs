#ifndef UART_DRIVER_H
#define UART_DRIVER_H

typedef struct uart_ops {
    void (*putc)(char);

    int (*getc)(void);

} uart_ops_t;

typedef struct uart_device {

    const uart_ops_t *ops;

} uart_device_t;

extern uart_device_t uart0;

#endif
