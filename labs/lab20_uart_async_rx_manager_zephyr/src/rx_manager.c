#include "rx_manager.h"

#include <errno.h>
#include <string.h>

#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

BUILD_ASSERT(!IS_ENABLED(CONFIG_SMP), "Lab20 uses single-core IRQ locking");

static int buffer_index(const struct rx_manager *mgr, const uint8_t *buf) {
    for (size_t i = 0; i < RX_DRIVER_COUNT; ++i) {
	if (buf == mgr->driver_buf[i])
            return (int)i;
    }

    return -1;
}

static void rx_uart_callback(const struct device *dev, struct uart_event *evt, void *user_data) {
    struct rx_manager *mgr = user_data;

    if (mgr != NULL && dev == mgr->dev)
        rx_manager_event(mgr, evt);
}

int rx_manager_init(struct rx_manager *mgr, const struct device *dev) {
    if (mgr == NULL || dev == NULL) 
	return -EINVAL;

    if (!device_is_ready(dev))
	return -ENODEV;

    memset(mgr, 0, sizeof(*mgr));
    mgr->dev = dev;

    ring_buf_init(&mgr->app_ring, sizeof(mgr->app_storage), mgr->app_storage);

    /* Async UART requires a callback before RX is enabled */
    int ret = uart_callback_set(dev, rx_uart_callback, mgr);

    if (ret != 0) {
        mgr->last_error = ret;
	return ret;
    }

    /* Transfer buffer A to the driver */
    mgr->driver_owned[0] = true;
    mgr->enabled = true;

    ret = uart_rx_enable(dev, mgr->driver_buf[0], RX_DRIVER_SIZE, SYS_FOREVER_US);

    if (ret != 0) {
        mgr->driver_owned[0] = false;
	mgr->enabled = false;
	mgr->last_error = ret;
        (void) uart_callback_set(dev, NULL, NULL);
    }

    return ret;
}


void rx_manager_event(struct rx_manager *mgr, const struct uart_event *evt) {
    if (mgr == NULL || evt == NULL)
	return;

    unsigned int key = irq_lock();

    switch (evt->type) {
	case UART_RX_RDY: {
	    int index = buffer_index(mgr, evt->data.rx.buf);
	    size_t offset = evt->data.rx.offset;
	    size_t len = evt->data.rx.len;

	    // verify that the event describes a valid span within one of our reusable driver buffers
	    if (index < 0 || !mgr->driver_owned[index] ||
                offset > RX_DRIVER_SIZE || len > RX_DRIVER_SIZE - offset) {
	        mgr->last_error = -EINVAL;
		break;
	    }

	    /* Zephyr RX_RDY semantics */
	    uint32_t copied = ring_buf_put(&mgr->app_ring, evt->data.rx.buf + offset, (uint32_t)len);

	    ++mgr->ready_events;
	    mgr->bytes_received += (uint32_t) len;
	    mgr->bytes_buffered += copied;
	    mgr->dropped_bytes += (uint32_t)len - copied;

	    break;
	}
	
	case UART_RX_BUF_REQUEST: {
	    int available = -1;

	    ++mgr->request_events;

	    for (size_t i = 0; i < RX_DRIVER_COUNT; ++i) {
                if (!mgr->driver_owned[i]) {
                    available = (int) i;
		    break;
		}
	    }

	    if (available < 0) {
	       mgr->last_error = -ENOBUFS;
	       break;
	    }

	    mgr->driver_owned[available] = true;

	    int ret = uart_rx_buf_rsp(mgr->dev, mgr->driver_buf[available], RX_DRIVER_SIZE);

	    if (ret != 0) {
		mgr->driver_owned[available] = false;
		mgr->last_error = ret;
	    }
	    break;
	}

	case UART_RX_BUF_RELEASED: {
	    int index = buffer_index(mgr, evt->data.rx_buf.buf);

	    if (index < 0 || !mgr->driver_owned[index]) {
                mgr->last_error = -EINVAL;
		break;
	    }

	    mgr->driver_owned[index] = false;
	    ++mgr->released_events; 

	    break;
	}

	case UART_RX_DISABLED:
	    mgr->enabled = false;
	    ++mgr->disabled_events;
	    break;

	case UART_RX_STOPPED:
	    ++mgr->stopped_events;
	    mgr->last_stop_reason = (uint32_t)evt->data.rx_stop.reason;
	    mgr->last_error = -EIO;
	    break;
	default:
	    break;
    }

    irq_unlock(key);
}

size_t rx_manager_read(struct rx_manager *mgr, uint8_t *dst, size_t max_len) {
    if (mgr == NULL || dst == NULL || max_len == 0U) {
        return 0U;
    }

    uint32_t limit = (uint32_t)MIN(max_len, (size_t)RX_RING_SIZE);
    unsigned int key = irq_lock();

    uint32_t count = ring_buf_get(&mgr->app_ring, dst, limit);
    mgr->bytes_read += count;
    irq_unlock(key);

    return count;
}
