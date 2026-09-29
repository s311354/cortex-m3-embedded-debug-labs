#define DT_DRV_COMPAT lab20_cmsdk_uart_async

#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cmsis_core.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include "uart_cmsdk_regs.h"

BUILD_ASSERT(!IS_ENABLED(CONFIG_SMP), "This lab supports one Cortex-M3 core");
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1, "Lab20 owns UART0 only");
BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, data) == 0x00);
BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, state) == 0x04);
BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, ctrl) == 0x08);
BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, intclear) == 0x0c);
BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, bauddiv) == 0x10);
BUILD_ASSERT(sizeof(struct uart_cmsdk_regs) == 0x14);
BUILD_ASSERT(UART_CMSDK_INT_RX == BIT(1));
BUILD_ASSERT(UART_CMSDK_CTRL_RXIRQEN == BIT(3));


enum lab20_rx_state {
    LAB20_RX_IDLE,
    LAB20_RX_ACTIVE,
    LAB20_RX_STOPPING
};

struct lab20_uart_config {
    struct uart_cmsdk_regs *regs;
    uint32_t clock_hz, baudrate;
    unsigned int rx_irq;
};

struct lab20_uart_data {
    uart_callback_t callback;
    void *user_data;
    enum lab20_rx_state state;
    bool in_callback;
    bool initial_request;
    bool request_outstanding;
    uint8_t *current_buf;
    size_t current_len, current_pos, reported;
    uint8_t *next_buf;
    size_t next_len;
    /* Includes software-pended start/stop IRQs, not just received bytes */
    uint32_t irq_entries, hardware_bytes, hardware_overruns;
};

static void lab20_uart_isr(const void *arg);

static void lab20_uart_emit(const struct device *dev, struct uart_event *evt) {
    struct lab20_uart_data *data = dev->data;
    data->in_callback = true;
    data->callback(dev, evt, data->user_data);
    data->in_callback = false;
}

static void request_buffer(const struct device *dev) {
    struct lab20_uart_data *data = dev->data;
    data->request_outstanding = true;
    struct uart_event evt = {
	    .type = UART_RX_BUF_REQUEST
    };
    lab20_uart_emit(dev, &evt);
}

static void report_current(const struct device *dev) {
    struct lab20_uart_data *data = dev->data;
    if (data->current_pos == data->reported) {
        return;
    }

    struct uart_event evt = {
        .type = UART_RX_RDY,
	.data.rx = {
	    .buf = data->current_buf,
	    .offset = data->reported,
	    .len = data->current_pos - data->reported,
	},
    };

    data->reported = data->current_pos;
    lab20_uart_emit(dev, &evt);
}

static void release_buffer(const struct device *dev, uint8_t *buf) {
    if (buf != NULL) {
        struct uart_event evt = {
	    .type = UART_RX_BUF_RELEASED,
	    .data.rx_buf = {
		.buf = buf
	    },
	};
	lab20_uart_emit(dev, &evt);
    }
}

static void finish_rx(const struct device *dev, int reason) {
    const struct lab20_uart_config *config = dev->config;

    struct lab20_uart_data *data = dev->data;
    config->regs->ctrl &= ~(UART_CMSDK_CTRL_RXEN |
		    	    UART_CMSDK_CTRL_RXIRQEN);

    config->regs->intclear = UART_CMSDK_INT_RX;

    data->state = LAB20_RX_STOPPING;
    data->initial_request = false;
    data->request_outstanding = false;

    if (reason != 0) {
        struct uart_event evt = {
	    .type = UART_RX_STOPPED,
	    .data.rx_stop = {
		.reason = (enum uart_rx_stop_reason) reason,
		.data = {
		    .buf = data->current_buf,
		    .offset = data->reported,
		    .len = data->current_pos - data->reported,
		},
	    },
	};

	lab20_uart_emit(dev, &evt);
    }

    report_current(dev);
    uint8_t *current = data->current_buf;
    uint8_t *next = data->next_buf;

    data->current_buf = NULL;
    data->next_buf = NULL;
    data->current_len = data->current_pos = data->reported = data->next_len = 0U;

    release_buffer(dev, current);
    release_buffer(dev, next);
    data->state = LAB20_RX_IDLE;

    struct uart_event evt = {
        .type = UART_RX_DISABLED
    };

    lab20_uart_emit(dev, &evt);
}

static int lab20_uart_callback_set(const struct device *dev, uart_callback_t callback, void *user_data) {
    struct lab20_uart_data *data = dev->data;

    unsigned int key = irq_lock();

    int ret = 0;

    if (data->state != LAB20_RX_IDLE || data->in_callback) {
        ret = -EBUSY;
    } else {
        data->callback = callback;
	data->user_data = user_data;
    }

    irq_unlock(key);
    return ret;
}

static int lab20_uart_rx_enable(const struct device *dev, uint8_t *buf, size_t len, int32_t timeout) {
    const struct lab20_uart_config *config = dev->config;

    struct lab20_uart_data *data = dev->data;

    if (buf == NULL || len == 0U) {
        return -EINVAL;
    }

    if (timeout != SYS_FOREVER_US) {
        return -ENOTSUP;
    }

    unsigned int key = irq_lock();

    if (data->state != LAB20_RX_IDLE || data->callback == NULL) {
        int ret = data->state != LAB20_RX_IDLE ? -EBUSY : -EINVAL;
	irq_unlock(key);
	return ret;
    }

    data->current_buf = buf;
    data->current_len = len;
    data->current_pos = data->reported = 0U;
    data->next_buf = NULL;
    data->next_len = 0U;
    data->request_outstanding = false;
    data->initial_request = true;
    data->state = LAB20_RX_ACTIVE;
    config->regs->intclear = UART_CMSDK_INT_RX;
    config->regs->ctrl |= UART_CMSDK_CTRL_RXEN |
	                  UART_CMSDK_CTRL_RXIRQEN;

    // First REQUEST and any pre-existing RXFULL are handled by the real ISR
    NVIC_SetPendingIRQ((IRQn_Type) config->rx_irq);
    irq_unlock(key);

    return 0;
}

static bool buffers_overlap(const uint8_t *a, size_t a_len, const uint8_t *b, size_t b_len) {
    uintptr_t x = (uintptr_t)a, y = (uintptr_t) b;

    return x <= y ? y - x < a_len : x - y < b_len;
}

static int lab20_uart_rx_buf_rsp(const struct device *dev, uint8_t *buf, size_t len) {
    struct lab20_uart_data* data = dev->data;

    if (buf == NULL || len == 0U)
	return -EINVAL;

    unsigned int key = irq_lock();

    int ret = 0;

    if (data->state != LAB20_RX_ACTIVE) {
        ret = -EACCES;
    } else if (data->next_buf != NULL || !data->request_outstanding) {
        ret = -EBUSY;
    } else if (buffers_overlap(buf, len, data->current_buf, data->current_len)) {
        ret = -EINVAL;
    } else {
        data->next_buf = buf;
	data->next_len = len;
	data->request_outstanding = false;
    }

    irq_unlock(key);
    return ret;
}

static int lab20_uart_rx_disable(const struct device *dev) {
    const struct lab20_uart_config *config = dev->config;
    struct lab20_uart_data *data = dev->data;

    unsigned int key = irq_lock();
    int ret = 0;

    if (data->state == LAB20_RX_IDLE) {
        ret = -EFAULT;
    } else if (data->state == LAB20_RX_STOPPING) {
        ret = -EALREADY;
    } else {
        data->state = LAB20_RX_STOPPING;
	config->regs->ctrl &= ~(UART_CMSDK_CTRL_RXEN | 
			        UART_CMSDK_CTRL_RXIRQEN);
	NVIC_SetPendingIRQ((IRQn_Type)config->rx_irq);
    }
    irq_unlock(key);

    return ret;
}

static void lab20_uart_isr(const void *arg) {
    const struct device *dev = arg;
    const struct lab20_uart_config *config = dev->config;

    struct lab20_uart_data *data = dev->data;
    struct uart_cmsdk_regs *regs = config->regs;

    unsigned int key = irq_lock();
    ++data->irq_entries;
    regs->intclear = UART_CMSDK_INT_RX;

    if (data->state == LAB20_RX_STOPPING) {
        finish_rx(dev, 0);
	goto out;
    }

    if (data->state != LAB20_RX_ACTIVE) {
        goto out;
    }

    if (data->initial_request) {
        data->initial_request = false;
	request_buffer(dev);

	if (data->state == LAB20_RX_STOPPING) {
	    finish_rx(dev, 0);
	    goto out;
	}
    }

    uint32_t status = regs->state;

    if ((status & UART_CMSDK_STATE_RXOR) != 0U ) {
        ++data->hardware_overruns;
	regs->intclear = UART_CMSDK_INT_RXOR;
	finish_rx(dev, UART_ERROR_OVERRUN);
	goto out;
    }

    if ((status & UART_CMSDK_STATE_RXBF) == 0U) {
        goto out;
    }

    data->current_buf[data->current_pos++] = (uint8_t) (regs->data & UART_CMSDK_DATA_MASK);

    ++data->hardware_bytes;

    if (data->current_pos < data->current_len)
	goto out;

    report_current(dev);

    if (data->state == LAB20_RX_STOPPING || data->next_buf == NULL) {
        finish_rx(dev, 0);
	goto out;
    }

    uint8_t *completed = data->current_buf;
    data->current_buf = data->next_buf;
    data->current_len = data->next_len;
    data->current_pos = data->reported = 0U;
    data->next_buf = NULL;
    data->next_len = 0U;

    release_buffer(dev, completed);

    if (data->state == LAB20_RX_ACTIVE) {
        request_buffer(dev);
    }

    if (data->state == LAB20_RX_STOPPING) {
        finish_rx(dev, 0);
    }

out:
    irq_unlock(key);
}


static int lab20_uart_tx(const struct device *dev, const uint8_t *buf, size_t len, int32_t timeout) {
    ARG_UNUSED(dev); ARG_UNUSED(buf); ARG_UNUSED(len); ARG_UNUSED(timeout);

    return -ENOTSUP;
}

static int lab20_uart_tx_abort(const struct device *dev) {
    ARG_UNUSED(dev);
    return -ENOTSUP;
}

static int lab20_uart_poll_in(const struct device *dev, unsigned char *c) {
    ARG_UNUSED(dev); ARG_UNUSED(c);
    return -ENOTSUP;
}

static void lab20_uart_poll_out(const struct device *dev, unsigned char c) {
    const struct lab20_uart_config *config = dev->config;

    while (config->regs->state & UART_CMSDK_STATE_TXBF) {
    }
    config->regs->data = (uint32_t)c;
}

static int lab20_uart_init(const struct device *dev) {
    const struct lab20_uart_config *config = dev->config;

    if (config->baudrate == 0U) {
        return -EINVAL;
    }

    uint32_t divisor = config->clock_hz / config->baudrate;

    if (divisor < UART_CMSDK_BAUDDIV_MIN || divisor > UART_CMSDK_BAUDDIV_MASK) {
        return -EINVAL;
    }

    config->regs->ctrl = 0U;
    config->regs->bauddiv = divisor;
    config->regs->intclear = UART_CMSDK_INT_RX | 
	                     UART_CMSDK_INT_RXOR;

    // RX remains off untill rx_enable. TX polling is optional and unused in main
    config->regs->ctrl = UART_CMSDK_CTRL_TXEN;

    IRQ_CONNECT(DT_INST_IRQ_BY_NAME(0, rx, irq),
		DT_INST_IRQ_BY_NAME(0, rx, priority),
		lab20_uart_isr, DEVICE_DT_INST_GET(0), 0);

    NVIC_ClearPendingIRQ((IRQn_Type)config->rx_irq);
    irq_enable(config->rx_irq);
    return 0;
}

static DEVICE_API(uart, lab20_uart_api) = {
    .poll_in = lab20_uart_poll_in,
    .poll_out = lab20_uart_poll_out,
    .callback_set = lab20_uart_callback_set,
    .tx = lab20_uart_tx,
    .tx_abort = lab20_uart_tx_abort,
    .rx_enable = lab20_uart_rx_enable,
    .rx_buf_rsp = lab20_uart_rx_buf_rsp,
    .rx_disable = lab20_uart_rx_disable,
};

static struct lab20_uart_data lab20_uart_data_0;
static const struct lab20_uart_config lab20_uart_config_0 = {
    .regs = (struct uart_cmsdk_regs *)DT_INST_REG_ADDR(0),
    .clock_hz = DT_INST_PROP_BY_PHANDLE(0, clocks, clock_frequency),
    .baudrate = DT_INST_PROP(0, current_speed),
    .rx_irq = DT_INST_IRQ_BY_NAME(0, rx, irq),
};

DEVICE_DT_INST_DEFINE(0, lab20_uart_init, NULL, &lab20_uart_data_0,
		     &lab20_uart_config_0, PRE_KERNEL_1,
		     CONFIG_SERIAL_INIT_PRIORITY, &lab20_uart_api);
