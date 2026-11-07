#include "dma_sim.h"

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <zephyr/drivers/dma.h>
#include <zephyr/sys/util.h>

struct dma_sim_data {
    const struct device *dev;
    struct k_timer timer;
    struct k_work complete_work;
    struct k_sem completion_sem;
    struct dma_config config;
    struct dma_block_config block;
    bool configured;
    bool running;
};

static struct dma_sim_data sim;

static void completed_work_handler(struct k_work *work) {
    struct dma_sim_data *data = CONTAINER_OF(work, struct dma_sim_data, complete_work);

    /* CPU memcpy is intentionally a simuation */
    memcpy((void *) (uintptr_t)data->block.dest_address, (const void *)(uintptr_t)data->block.source_address, data->block.block_size);

    data->running = false;
    data->config.dma_callback(data->dev, data->config.user_data, 0U, DMA_STATUS_COMPLETE);

    k_sem_give(&data->completion_sem);
}

static void timer_handler(struct k_timer *timer) {
    struct dma_sim_data *data = CONTAINER_OF(timer, struct dma_sim_data, timer);

    /* This timer expires in interrupt context */
    (void) k_work_submit(&data->complete_work);
}

static int sim_configure(const struct device *dev, uint32_t channel, struct dma_config *config) {
    struct dma_sim_data *data = dev->data;

    if (channel != 0U || config == NULL || config->head_block == NULL || 
	config->block_count != 1U || config->channel_direction != MEMORY_TO_MEMORY ||
	config->dma_callback == NULL) {
        return -EINVAL;
    }

    if (data->running)
	return -EBUSY;

    data->config = *config;
    data->block = *config->head_block;
    data->configured = true;

    return 0;
}

static int sim_start(const struct device *dev, uint32_t channel) {
    struct dma_sim_data *data = dev->data;

    if (channel != 0U || !data->configured) {
        return -EINVAL;
    }

    if (data->running)
	return -EBUSY;

    data->running = true;
    k_timer_start(&data->timer, K_MSEC(15), K_NO_WAIT);

    return 0;
}

static int sim_init(const struct device *dev) {
    struct dma_sim_data *data = dev->data;

    data->dev = dev;
    data->running = false;
    data->configured = false;

    k_timer_init(&data->timer, timer_handler, NULL);
    k_work_init(&data->complete_work, completed_work_handler);
    k_sem_init(&data->completion_sem, 0U, 16U);

    return 0;
}

static DEVICE_API(dma, sim_api) = {
    .config = sim_configure,
    .start = sim_start,
};

DEVICE_DEFINE(lab26_dma, "lab26_dma", sim_init, NULL, 
	      &sim, NULL, 
	      POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEVICE, &sim_api);

const struct device *dma_sim_device(void) {
    return DEVICE_GET(lab26_dma);
}

int dma_sim_wait(k_timeout_t timeout) {
    return k_sem_take(&sim.completion_sem, timeout);
}
