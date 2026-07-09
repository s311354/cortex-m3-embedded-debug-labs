#ifndef UART_DRIVER_H
#define UART_DRIVER_H

struct uart_driver_ops {
    void (*init)(void);

    void (*putc)(char c);

    char (*getc)(void);

};


struct uart_device {
    const struct uart_driver_ops *ops;
};

static inline void uart_driver_init(struct uart_device *dev) {
    return dev->ops->init();
}

static inline void uart_driver_putc(struct uart_device *dev, char c) {
    dev->ops->putc(c);
}

static inline int uart_driver_getc(struct uart_device *dev) {
    return dev->ops->getc();
}

#endif
