#include "dma_queue.h"
#include "dma_sim.h"

#include <errno.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#define TRANSFER_COUNT 5U
#define BYTES_PER_TRANSFER 8U

static struct dma_queue queue;
static uint8_t sources[TRANSFER_COUNT + 1U][BYTES_PER_TRANSFER];
static uint8_t destinations[TRANSFER_COUNT + 1U][BYTES_PER_TRANSFER];

volatile unsigned int lab26_stage;
volatile unsigned int last_stage;
volatile int lab26_last_rc;

__attribute__((noinline)) void lab26_checkpoint(void) {
    __asm__ volatile ("nop");
}

int main(void) {
    const struct device *dev = dma_sim_device();

    /* Stage 1. Init */
    lab26_stage = 1U;
    lab26_last_rc = dma_queue_init(&queue, dev, 0U);
    if (lab26_last_rc != 0) {
        printk("LAB26 FAIL init=%d\n", lab26_last_rc);
	return 1;
    }

    printk("LAB26 INIT channel=0 pending_capacity=4\n");
    lab26_checkpoint();

    for (unsigned int i = 0U; i < TRANSFER_COUNT + 1U; ++i) {
        memset(sources[i], (int)(0x41U + i), BYTES_PER_TRANSFER);
    }

    /* Stage 2. Submit */
    lab26_stage = 2U;
    for (unsigned int i = 0U; i < TRANSFER_COUNT; ++i) {
        lab26_last_rc = dma_queue_submit(&queue, sources[i], destinations[i], BYTES_PER_TRANSFER);

	if (lab26_last_rc != 0) {
	    printk("LAB26 FAIL submit[%u]=%d\n", i, lab26_last_rc);
	    return 1;
	}
    }

    printk("LAB26 QUEUED active=1 waiting=%u\n", queue.count);
    lab26_checkpoint();

    lab26_last_rc = dma_queue_submit(&queue, sources[5], destinations[5], BYTES_PER_TRANSFER);

    if (lab26_last_rc != -ENOSPC) {
        printk("LAB26 FAIL full=%d\n", lab26_last_rc);
	return 1;
    }
    printk("LAB26 FULL rejected=%d\n", lab26_last_rc);

    /* Stage 3. Wait */
    lab26_stage = 3U;
    for (unsigned int i = 0U; i < TRANSFER_COUNT; ++i) {
        if (dma_sim_wait(K_SECONDS(2)) != 0) {
	    printk("LAB26 FAIL timeout at transfer=%u\n", i);
	    return 1;
	}
    }
    lab26_checkpoint();

    for (unsigned int i = 0U; i < TRANSFER_COUNT; ++i) {
        if (memcmp(sources[i], destinations[i], BYTES_PER_TRANSFER) != 0) {
	    printk("LAB26 FAIL data mismatch at transfer=%u\n", i);
	    return 1;
	}
    }

    if (queue.completed != TRANSFER_COUNT || queue.failed != 0U ||
	queue.busy || queue.count != 0U) {
        printk("LAB26 FAIL state: done=%u failed=%u busy=%u queued=%u\n", queue.completed, queue.failed, 
		(unsigned int)queue.busy, queue.count);
	return 1;
    }

    /* Stage 4. Complete */
    last_stage = 4U;
    printk("LAB26 PASS completed=%u failed=%u FIFO=5\n", queue.completed, queue.failed);
    lab26_checkpoint();

    return 0;

}
