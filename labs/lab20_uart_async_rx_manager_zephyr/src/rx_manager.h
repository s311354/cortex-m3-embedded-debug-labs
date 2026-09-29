#ifndef LAB20_RX_MANAGER_H
#define LAB20_RX_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/ring_buffer.h>

#define RX_DRIVER_COUNT 2U
#define RX_DRIVER_SIZE  8U
#define RX_RING_SIZE    64U

struct rx_manager {
    const struct device *dev;

    /* Temporary buffers whose ownership is transferred to the UART driver */
    uint8_t driver_buf[RX_DRIVER_COUNT][RX_DRIVER_SIZE];

    bool driver_owned[RX_DRIVER_COUNT];

    /* Persistent application-owned queue */
    struct ring_buf app_ring;
    uint8_t app_storage[RX_RING_SIZE];

    bool enabled;

    uint32_t ready_events;
    uint32_t request_events;
    uint32_t released_events;
    uint32_t disabled_events;
    uint32_t stopped_events;

    uint32_t bytes_received; // number of bytes reported by UART_RX_RDY
    uint32_t bytes_buffered; // number successfully copied into app_ring
    uint32_t bytes_read;     // number consumed by rx_manager_read()

    uint32_t dropped_bytes;
    uint32_t last_stop_reason;
    int last_error;
};

int rx_manager_init(struct rx_manager *mgr, const struct device *dev);

void rx_manager_event(struct rx_manager *mgr, const struct uart_event *evt);

size_t rx_manager_read(struct rx_manager *mgr, uint8_t *dst, size_t max_len);

#endif
