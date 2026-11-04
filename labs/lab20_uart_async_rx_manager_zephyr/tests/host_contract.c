#include "rx_manager.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *group = "startup";
static unsigned long assertions, cases;

#define CHECK(expr) do { \
	++assertions; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL [%s] %s:%d %s\n", group, __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

struct fake_uart {
    uart_callback_t callback;
    void *user_data;
    int callback_error, enable_error, response_error;
    unsigned int callback_calls, enable_calls, response_calls;
    uint8_t *first_buffer, *response_buffer;
    size_t first_size, response_size;
    int32_t timeout;
};

static struct device dev;
static struct fake_uart uart;
static unsigned int irq_depth;

unsigned int irq_lock(void) {
    unsigned int previous = irq_depth;

    ++irq_depth;

    return previous;
}

void irq_unlock(unsigned int previous) {
    CHECK(irq_depth == previous + 1U);

    irq_depth = previous;
}

int uart_callback_set(const struct device *d, uart_callback_t cb, void *user) {
    CHECK(d == &dev);

    ++uart.callback_calls;

    if (cb != NULL && uart.callback_error != 0) {
        return uart.callback_error;
    }

    uart.callback = cb;
    uart.user_data = user;

    return 0;
}

int uart_rx_enable(const struct device *d, uint8_t *buffer, size_t length, int32_t timeout) {
    CHECK(d == &dev);

    ++uart.enable_calls;

    uart.first_buffer = buffer;
    uart.first_size = length;
    uart.timeout = timeout;

    return uart.enable_error;
}

int uart_rx_buf_rsp(const struct device *d, uint8_t *buffer, size_t length) {
    CHECK(d == &dev);

    ++uart.response_calls;

    uart.response_buffer = buffer;
    uart.response_size = length;

    return uart.response_error;
}

static void reset(void) {
    memset(&uart, 0, sizeof(uart));

    dev.ready = true;
    irq_depth = 0U;
}

static void init(struct rx_manager *mgr) {
    CHECK(rx_manager_init(mgr, &dev) == 0);
    CHECK(uart.callback != NULL && uart.user_data == mgr);
    CHECK(uart.callback_calls == 1U && uart.enable_calls == 1U);
    CHECK(uart.first_buffer == mgr->driver_buf[0]);
    CHECK(uart.first_size == RX_DRIVER_SIZE);
    CHECK(uart.timeout == SYS_FOREVER_US);
    CHECK(mgr->enabled && mgr->driver_owned[0] && !mgr->driver_owned[1]);
    CHECK(ring_buf_size_get(&mgr->app_ring) == 0U);
    CHECK(irq_depth == 0U);
}

static void emit(struct uart_event event) {
    CHECK(uart.callback != NULL);

    uart.callback(&dev, &event, uart.user_data);

    CHECK(irq_depth == 0U);
}

static void request(void) {
    struct uart_event event = {
        .type = UART_RX_BUF_REQUEST
    };

    emit(event);
}

static void release(uint8_t *buffer) {
    struct uart_event event = {
        .type = UART_RX_BUF_RELEASED
    };

    event.data.rx_buf.buf = buffer;

    emit(event);
}

static void ready(uint8_t *buffer, size_t offset, size_t length) {
    struct uart_event event = {
        .type = UART_RX_RDY
    };

    event.data.rx.buf = buffer;
    event.data.rx.offset = offset;
    event.data.rx.len = length;

    emit(event);
}

static void deliver_eight(struct rx_manager *mgr, unsigned int current, unsigned int base) {
    CHECK(current < RX_DRIVER_COUNT && mgr->driver_owned[current]);

    for (size_t j = 0; j < RX_DRIVER_SIZE; ++j) {
        mgr->driver_buf[current][j] = (uint8_t) (base + (unsigned int)j);
    }

    ready(mgr->driver_buf[current], 0U, RX_DRIVER_SIZE);
    release(mgr->driver_buf[current]);
    CHECK(!mgr->driver_owned[current]);

    request();
    CHECK(uart.response_buffer == mgr->driver_buf[current]);
    CHECK(mgr->driver_owned[current]);
}

static void test_init_and_read_contract(void) {
    reset();
    struct rx_manager mgr;
    uint8_t out[8] = {0x7b};

    CHECK(rx_manager_init(NULL, &dev) == -EINVAL);
    CHECK(rx_manager_init(&mgr, NULL) == -EINVAL);
    CHECK(uart.callback_calls == 0U);

    dev.ready = false;
    CHECK(rx_manager_init(&mgr, &dev) == -ENODEV);
    CHECK(uart.callback_calls == 0U);

    dev.ready = true;
    uart.callback_error = -EIO;
    CHECK(rx_manager_init(&mgr, &dev) == -EIO);
    CHECK(mgr.last_error == -EIO && !mgr.enabled);
    CHECK(uart.enable_calls == 0U);

    reset();
    uart.enable_error = -EBUSY;
    CHECK(rx_manager_init(&mgr, &dev) == -EBUSY);
    CHECK(!mgr.enabled && !mgr.driver_owned[0]);
    CHECK(mgr.last_error == -EBUSY);
    CHECK(uart.callback == NULL && uart.callback_calls == 2U);

    reset();
    init(&mgr);
    CHECK(rx_manager_read(NULL, out, sizeof(out)) == 0U);
    CHECK(rx_manager_read(&mgr, NULL, sizeof(out)) == 0U);
    CHECK(rx_manager_read(&mgr, out, 0U) == 0U);
    CHECK(rx_manager_read(&mgr, out, sizeof(out)) == 0U);
    CHECK(out[0] == 0x7b && mgr.bytes_read == 0U);
    ++cases;
}

static void test_two_buffer_ownership(void) {
    reset();
    struct rx_manager mgr;

    init(&mgr);
    request();
    CHECK(mgr.driver_owned[0] && mgr.driver_owned[1]);
    CHECK(uart.response_calls == 1U);
    CHECK(uart.response_buffer == mgr.driver_buf[1]);
    CHECK(uart.response_size == RX_DRIVER_SIZE);

    request();
    CHECK(mgr.last_error == -ENOBUFS);
    CHECK(mgr.request_events == 2U && uart.response_calls == 1U);

    release(mgr.driver_buf[0]);
    CHECK(!mgr.driver_owned[0] && mgr.released_events == 1U);

    request();
    CHECK(mgr.driver_owned[0] && uart.response_buffer == mgr.driver_buf[0]);

    release(mgr.driver_buf[1]);
    request();
    CHECK(mgr.driver_owned[1] && uart.response_buffer == mgr.driver_buf[1]);
    CHECK(mgr.released_events == 2U && mgr.request_events == 4U);
    ++cases;
}

static void test_response_error_rollback(void) {
    reset();
    struct rx_manager mgr;

    init(&mgr);
    uart.response_error = -EAGAIN;
    request();
    CHECK(!mgr.driver_owned[1] && mgr.driver_owned[0]);
    CHECK(mgr.last_error == -EAGAIN && uart.response_calls == 1U);

    uart.response_error = 0U;
    request();
    CHECK(mgr.driver_owned[1] && uart.response_calls == 2U);
    CHECK(uart.response_buffer == mgr.driver_buf[1]);
    ++cases;
}

static void test_ready_and_invalid_events(void) {
    reset();
    struct rx_manager mgr;
    uint8_t out[8] = {0};
    uint8_t foreign[8] = {0};

    init(&mgr);
    memcpy(mgr.driver_buf[0], "ABCDEFGH", RX_DRIVER_SIZE);
    ready(mgr.driver_buf[0], 2U, 4U);
    CHECK(mgr.ready_events == 1U && mgr.bytes_received == 4U);
    CHECK(mgr.bytes_buffered == 4U && mgr.dropped_bytes == 0U);
    CHECK(rx_manager_read(&mgr, out, 2U) == 2U);
    CHECK(memcmp(out, "CD", 2U) == 0);
    CHECK(rx_manager_read(&mgr, out, sizeof(out)) == 2U);
    CHECK(memcmp(out, "EF", 2U) == 0);
    CHECK(mgr.bytes_read == 4U);

    ready(foreign, 0U, 1U);
    CHECK(mgr.last_error == -EINVAL && mgr.ready_events == 1U);
    
    ready(mgr.driver_buf[1], 0U, 1U);
    CHECK(mgr.last_error == -EINVAL && mgr.bytes_received == 4U);

    ready(mgr.driver_buf[0], 9U, 0U);
    ready(mgr.driver_buf[0], 7U, 2U);
    CHECK(mgr.ready_events == 1U && mgr.bytes_buffered == 4U);

    release(foreign);
    CHECK(mgr.last_error == -EINVAL && mgr.released_events == 0U);

    release(mgr.driver_buf[0]);
    CHECK(mgr.released_events == 1U && !mgr.driver_owned[0]);

    release(mgr.driver_buf[0]);
    CHECK(mgr.last_error == -EINVAL && mgr.released_events == 1U);

    ready(mgr.driver_buf[0], 0U, 1U);
    CHECK(mgr.ready_events == 1U && mgr.bytes_received == 4U);

    rx_manager_event(NULL, NULL);
    rx_manager_event(&mgr, NULL);
    CHECK(irq_depth == 0U);
    ++cases;
}

static void test_ring_wrap_and_partial_reads(void) {
    reset();
    struct rx_manager mgr;
    uint8_t out[80];

    init(&mgr);
    request();

    for (unsigned int chunk = 0; chunk < 8U; ++chunk) {
        deliver_eight(&mgr, chunk % 2U, chunk * 8U);
    }

    CHECK(ring_buf_size_get(&mgr.app_ring) == RX_RING_SIZE);
    CHECK(rx_manager_read(&mgr, out, 50U) == 50U);

    for (size_t i = 0U; i < 50U; ++i) {
        CHECK(out[i] == i);
    }

    deliver_eight(&mgr, 0U, 64U);
    deliver_eight(&mgr, 1U, 72U);
    CHECK(ring_buf_size_get(&mgr.app_ring) == 30U);

    memset(out, 0, sizeof(out));
    CHECK(rx_manager_read(&mgr, out, sizeof(out)) == 30U);

    for (size_t i = 0U; i < 30U; ++i) {
        CHECK(out[i] == i + 50U);
    }

    CHECK(rx_manager_read(&mgr, out, sizeof(out)) == 0U);
    CHECK(mgr.bytes_received == 80U && mgr.bytes_buffered == 80U);
    CHECK(mgr.bytes_read == 80U && mgr.dropped_bytes == 0U);
    CHECK(mgr.ready_events == 10U && mgr.released_events == 10U);
    ++cases;
}

static void test_overflow_no_overwrite(void) {
    reset();
    struct rx_manager mgr;
    uint8_t out[RX_RING_SIZE];

    init(&mgr);
    request();

    for (unsigned int chunk = 0; chunk < 9U; ++chunk) {
        deliver_eight(&mgr, chunk % 2U, chunk * 8U);
    }

    CHECK(mgr.bytes_received == 72U);
    CHECK(mgr.bytes_buffered == 64U && mgr.dropped_bytes == 8U);
    CHECK(ring_buf_size_get(&mgr.app_ring) == 64U);
    CHECK(rx_manager_read(&mgr, out, sizeof(out)) == 64U);

    for (size_t i = 0U; i < sizeof(out); ++i) {
        CHECK(out[i] == i);
    }

    CHECK(mgr.bytes_read == 64U && ring_buf_size_get(&mgr.app_ring) == 0U);
    ++cases;
}

static void test_stopped_disabled_and_released(void) {
    reset();
    struct rx_manager mgr;

    init(&mgr);
    request();

    struct uart_event stopped = {
        .type = UART_RX_STOPPED
    };

    stopped.data.rx_stop.reason = UART_ERROR_OVERRUN;
    emit(stopped);
    CHECK(mgr.stopped_events == 1U && mgr.last_error == -EIO);
    CHECK(mgr.last_stop_reason == (uint32_t) UART_ERROR_OVERRUN);

    release(mgr.driver_buf[0]);
    release(mgr.driver_buf[1]);
    CHECK(!mgr.driver_owned[0] && !mgr.driver_owned[1]);

    emit((struct uart_event){ .type = UART_RX_DISABLED });
    CHECK(!mgr.enabled && mgr.disabled_events == 1U);
    CHECK(mgr.released_events == 2U);
    ++cases;
}

int main(void) {
    const struct {
        const char *name;
	void (*run) (void);
    } tests[] = {
	{"initialization and read contract", test_init_and_read_contract},
	{"two-buffer ownership and reuse", test_two_buffer_ownership},
	{"buffer response failure rollback", test_response_error_rollback},
	{"RX_RDY spans and invalid events", test_ready_and_invalid_events},
	{"ring wrap and partial reads", test_ring_wrap_and_partial_reads},
	{"ring overflow and old data preservation", test_overflow_no_overwrite},
	{"stop, release and disable", test_stopped_disabled_and_released},
    };

    for (size_t i = 0U; i < sizeof(tests)/sizeof(tests[0]); ++i) {
        group = tests[i].name;
	tests[i].run();
	printf("PASS: %s\n", group);
    }

    printf("PASS: %lu cases; %lu assertions\n", cases, assertions);
    puts("Host UART RX-manager contract only; not a Zephyr ISR or hardware test");
    return EXIT_SUCCESS;
}

