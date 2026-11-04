#ifndef HOST_ZEPHYR_DRIVERS_UART_H
#define HOST_ZEPHYR_DRIVERS_UART_H

#include <stddef.h>
#include <stdint.h>
#include <zephyr/device.h>

enum uart_event_type {
    UART_TX_DONE,
    UART_TX_ABORTED,
    UART_RX_RDY,
    UART_RX_BUF_REQUEST,
    UART_RX_BUF_RELEASED,
    UART_RX_DISABLED,
    UART_RX_STOPPED
};

enum uart_rx_stop_reason {
    UART_ERROR_OVERRUN = 1
};

struct uart_event {
    enum uart_event_type type;

    union {
	struct {
	    uint8_t *buf;
	    size_t offset;
	    size_t len; 
	} rx;
	
	struct {
	    uint8_t *buf;
	} rx_buf;

	struct {
            enum uart_rx_stop_reason reason;
	} rx_stop;
    } data;
};

typedef void (*uart_callback_t) (const struct device *, struct uart_event *, void *);

int uart_callback_set(const struct device *, uart_callback_t, void *);
int uart_rx_enable(const struct device *, uint8_t *, size_t, int32_t);
int uart_rx_buf_rsp(const struct device *, uint8_t *, size_t);

#define SYS_FOREVER_US (-1)

#endif
