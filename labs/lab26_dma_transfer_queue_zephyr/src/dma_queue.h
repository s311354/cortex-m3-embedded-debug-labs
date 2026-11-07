#ifndef LAB26_DMA_QUEUE_H_
#define LAB26_DMA_QUEUE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/dma.h>

#define DMA_QUEUE_CAPACITY 4U

struct dma_request {
    const void *src;
    void *dst;
    size_t len;
};

/* Single-channel */
struct dma_queue {
    const struct device *dev;
    uint32_t channel;
    struct dma_request pending[DMA_QUEUE_CAPACITY];
    unsigned int head;
    unsigned int count;
    bool busy;

    /* Persistent descriptors */
    struct dma_block_config block;
    struct dma_config config;

    unsigned int completed;
    unsigned int failed;
    int last_status;
};

int dma_queue_init(struct dma_queue *queue, const struct device *dev, uint32_t channel);
int dma_queue_submit(struct dma_queue *queue, const void *src, void *dst, size_t len);
/* call only in a serialized thread/workqueue context */
void dma_queue_on_complete(struct dma_queue *queue, int status);

#endif
