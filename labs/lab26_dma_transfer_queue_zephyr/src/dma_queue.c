#include "dma_queue.h"

#include <errno.h>
#include <limits.h>
#include <stdint.h>

static int start_next(struct dma_queue *queue);

static void completion_callback(const struct device *dev, void *user_data, uint32_t channel, int status) {
    struct dma_queue *queue = user_data;

    if (queue != NULL && dev == queue->dev && channel == queue->channel) {
        dma_queue_on_complete(queue, status);
    }
}

int dma_queue_init(struct dma_queue *queue, const struct device *dev, uint32_t channel) {
    if (queue == NULL || dev == NULL)
        return -EINVAL;

    if (!device_is_ready(dev))
	return -ENODEV;

    *queue = (struct dma_queue){0};

    queue->dev = dev;
    queue->channel = channel;

    queue->block.source_addr_adj = DMA_ADDR_ADJ_INCREMENT;
    queue->block.dest_addr_adj = DMA_ADDR_ADJ_INCREMENT;

    queue->config.channel_direction = MEMORY_TO_MEMORY;
    queue->config.source_data_size = 1U;
    queue->config.dest_data_size = 1U;
    queue->config.source_burst_length = 1U;
    queue->config.dest_burst_length = 1U;
    queue->config.block_count = 1U;
    queue->config.head_block = &queue->block;
    queue->config.dma_callback = completion_callback;
    queue->config.user_data = queue;

    return 0;
}

/* The only routine that transfers ownership of a queued request to DMA */
static int start_next(struct dma_queue *queue) {
    int first_error = 0;

    while (!queue->busy && queue->count != 0U) {
        const struct dma_request req = queue->pending[queue->head];

	queue->head = (queue->head + 1U) % DMA_QUEUE_CAPACITY;
	--queue->count;

	queue->block.source_address = (uintptr_t)req.src;
	queue->block.dest_address = (uintptr_t)req.dst;
	queue->block.block_size = (uint32_t)req.len;

	/* Claim the channel before strating an asynchronous operation */
	queue->busy = true;

	int rc = dma_config(queue->dev, queue->channel, &queue->config);
	if (rc == 0) {
	    rc = dma_start(queue->dev, queue->channel);
	}

	if (rc == 0) {
            return first_error;	
	}

	/* A rejected request is dropped and reported through counters */
	queue->busy = false;
	queue->last_status = rc;
	++queue->failed;

	if (first_error == 0)
	    first_error = rc;
    }

    return first_error;
}

int dma_queue_submit(struct dma_queue *queue, const void *src, void *dst, size_t len) {
    if (queue == NULL || queue->dev == NULL || src == NULL || dst == NULL || len == 0U)
	return -EINVAL;

#if SIZE_MAX > UINT32_MAX
    if (len > UINT32_MAX)
	return -E2BIG;
#endif

#ifndef CONFIG_DMA_64BIT
    /* Zephyr's 32-bit DMA descriptors must represent the full address span */
    const uintptr_t source = (uintptr_t) src;
    const uintptr_t destination = (uintptr_t) dst;
#if UINTPTR_MAX > UINT32_MAX
    if (source > UINT32_MAX || destionation > UINT32_MAX) {
        return -ERANGE;
    }
#endif
    if (len -1U > UINT32_MAX - source || len - 1U > UINT32_MAX - destination) {
        return -ERANGE;
    }
#endif

    if (queue->count == DMA_QUEUE_CAPACITY)
	return -ENOSPC;

    const unsigned int tail = (queue->head + queue->count) % DMA_QUEUE_CAPACITY;

    queue->pending[tail] = (struct dma_request) {src, dst, len};
    ++queue->count;

    return queue->busy ? 0 : start_next(queue);
}

void dma_queue_on_complete(struct dma_queue *queue, int status) {
     if (queue == NULL || !queue->busy || status > DMA_STATUS_COMPLETE) {
         return;
     }

     queue->busy = false;
     queue->last_status = status;

     if (status == DMA_STATUS_COMPLETE) {
         ++queue->completed;
     } else {
         ++queue->failed;
     }

     (void) start_next(queue);
}
