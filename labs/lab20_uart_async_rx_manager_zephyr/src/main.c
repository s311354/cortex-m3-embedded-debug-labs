#include "rx_manager.h"
#include <zephyr/devicetree.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>

static const struct device *const async_uart = DEVICE_DT_GET(DT_NODELABEL(uart0));
struct rx_manager g_rx_manager;

volatile bool g_consume_enabled = true;
volatile bool g_stop_requested;
volatile int g_init_rc;
volatile int g_stop_rc;

uint8_t g_last_read[16];
size_t g_last_read_len;
uint32_t g_total_read;
uint32_t g_ring_used;

__attribute__((noinline)) 
void lab20_ready(void) { 
    __asm__ volatile("" ::: "memory");
}

__attribute__((noinline))
void lab20_after_read(void) {
    __asm__ volatile("" ::: "memory");
}

int main(void) {
    g_init_rc = rx_manager_init(&g_rx_manager, async_uart);

    lab20_ready();

    if (g_init_rc != 0) {
        for (;;) {
	    k_sleep(K_FOREVER);
	}
    }

    for (;;) {
        if (g_stop_requested) {
	    g_stop_requested = false;
	    g_stop_rc = uart_rx_disable(async_uart);
	}

	if (g_consume_enabled) {
	    size_t n = rx_manager_read(&g_rx_manager, g_last_read, sizeof(g_last_read));

	    if (n != 0U) {
	        g_last_read_len = n;
		g_total_read += (uint32_t) n;
		lab20_after_read();
	    }
	}

	unsigned int key = irq_lock();
	g_ring_used = ring_buf_size_get(&g_rx_manager.app_ring);
	irq_unlock(key);
	k_sleep(K_MSEC(20));
    }

    return 0;
}
