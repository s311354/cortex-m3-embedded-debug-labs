#include "dma_queue.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long cases;
static unsigned long assertions;

#define CHECK(expr) do { \
	++assertions; \
	if (!(expr)) { \
		fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #expr); \
		exit(EXIT_FAILURE); \
	} \
} while (0)

static struct device device = {
    .ready = true,
};

static struct {
    struct dma_config config;
    struct dma_block_config block;
    uintptr_t started_sources[16];
    unsigned int configured;
    unsigned int started;
    unsigned int callback_calls;
    int config_error_once;
    int start_error_once;
    bool running;
} fake;

int dma_config(const struct device *dev, int32_t channel, struct dma_config *cfg) {
    CHECK(dev == &device && channel == 0U);
    CHECK(cfg != NULL && cfg->head_block != NULL);
    CHECK(cfg->channel_direction == MEMORY_TO_MEMORY);
    CHECK(cfg->block_count == 1U && cfg->dma_callback != NULL);
    CHECK(cfg->source_data_size == 1U && cfg->dest_data_size == 1U);
    CHECK(!fake.running);

    ++fake.configured;

    if (fake.config_error_once != 0) {
        int rc = fake.config_error_once;
	fake.config_error_once = 0;
	return rc;
    }

    fake.config = *cfg;
    fake.block = *cfg->head_block;
    return 0;
}

int dma_start(const struct device *dev, uint32_t channel) {
    CHECK(dev == &device && channel == 0U && !fake.running);

    if (fake.start_error_once != 0) {
        int rc = fake.start_error_once;
	fake.start_error_once = 0;
	return rc;
    }

    CHECK(fake.started < 16U);

    fake.started_sources[fake.started++] = fake.block.source_address;
    fake.running = true;
    return 0;
}

static void reset(void) {
    memset(&fake, 0, sizeof(fake));
    device.ready = true;
}

static void emit(int status) {
    CHECK(fake.config.dma_callback != NULL);
    CHECK(fake.running);

    /* Intermediate callbacks do not finish the active DMA request */
    if (status <= DMA_STATUS_COMPLETE)
	fake.running = false;

    ++fake.callback_calls;
    fake.config.dma_callback(&device, fake.config.user_data, 0U, status);
}

static void test_invalid_inputs(void) {
    struct dma_queue queue = {0};
    uint8_t src = 1U, dst = 0U;
    reset();

    CHECK(dma_queue_init(NULL, &device, 0U) == -EINVAL);
    CHECK(dma_queue_init(&queue, NULL, 0U) == -EINVAL);

    device.ready = false;
    CHECK(dma_queue_init(&queue, &device, 0U) == -ENODEV);

    device.ready = true;
    CHECK(dma_queue_init(&queue, &device, 0U) == 0);
    CHECK(dma_queue_submit(NULL, &src, &dst, 1U) == -EINVAL);
    CHECK(dma_queue_submit(&queue, NULL, &dst, 1U) == -EINVAL);
    CHECK(dma_queue_submit(&queue, &src, NULL, 1U) == -EINVAL);
    CHECK(dma_queue_submit(&queue, &src, &dst, 0U) == -EINVAL);

#if SIZE_MAX > UINT32_MAX
    CHECK(dma_queue_submit(&queue, &src, &dst, (size_t)UINT32_MAX + 1U) == -E2BIG);
#endif
    CHECK(fake.configured == 0U && fake.started == 0U);
    ++cases;
}

static void test_fifo_full_and_intermediate_events(void) {
    struct dma_queue queue;
    uint8_t source[7][4] = {{0}};
    uint8_t dest[7][4] = {{0}};
    reset();

    CHECK(dma_queue_init(&queue, &device, 0U) == 0);

    /* First request runs */
    for (unsigned int i = 0U; i < 5U; ++i) {
        CHECK(dma_queue_submit(&queue, source[i], dest[i], 4U) == 0);
    }

    CHECK(queue.busy && queue.count == 4U);
    CHECK(fake.started == 1U);
    CHECK(dma_queue_submit(&queue, source[5], dest[5], 4U) == -ENOSPC);
    CHECK(queue.count == 4U);

    emit(DMA_STATUS_BLOCK);
    emit(DMA_STATUS_HALF_COMPLETE);
    CHECK(queue.busy && queue.count == 4U && queue.completed == 0U);
    CHECK(fake.started == 1U);

    for (unsigned int i = 0U; i < 5U; ++i) {
        CHECK(fake.started_sources[i] == (uintptr_t) source[i]);
	emit(DMA_STATUS_COMPLETE);
	CHECK(queue.completed == i + 1U);
	CHECK(queue.failed == 0U);
	CHECK(fake.started == (i < 4U ? i + 2U : 5U));
    }

    CHECK(!queue.busy && queue.count == 0U && fake.started == 5U);
    CHECK(queue.last_status == DMA_STATUS_COMPLETE);
    CHECK(fake.configured == 5U);

    /* A new request wraps the ring-buffer tail without moving entries */
    CHECK(dma_queue_submit(&queue, source[6], dest[6], 4U) == 0);
    CHECK(fake.started == 6U && queue.busy);

    emit(DMA_STATUS_COMPLETE);
    CHECK(queue.completed == 6U && !queue.busy && queue.count == 0U);
    ++cases;
}

static void test_error_completion_advances_queue(void) {
    struct dma_queue queue;
    uint8_t source[2] = {1, 2};
    uint8_t dest[2] = {0};
    reset();

    CHECK(dma_queue_init(&queue, &device, 0U) == 0);
    CHECK(dma_queue_submit(&queue, &source[0], &dest[0], 1U) == 0);
    CHECK(dma_queue_submit(&queue, &source[1], &dest[1], 1U) == 0);

    emit(-EIO);
    CHECK(queue.failed == 1U && queue.completed == 0U);
    CHECK(queue.last_status == -EIO);
    CHECK(queue.busy && queue.count == 0U && fake.started == 2U);
    CHECK(fake.started_sources[1] == (uintptr_t)&source[1]);

    emit(DMA_STATUS_COMPLETE);
    CHECK(queue.completed == 1U && queue.failed == 1U && !queue.busy);
    dma_queue_on_complete(&queue, -EIO);
    CHECK(queue.failed == 1U);
    ++cases;
}

static void test_synchronous_driver_failures(void) {
    struct dma_queue queue;
    uint8_t source[3] = {1, 2, 3};
    uint8_t dest[3] = {0};
    reset();

    CHECK(dma_queue_init(&queue, &device, 0U) == 0);

    fake.config_error_once = -EIO;
    CHECK(dma_queue_submit(&queue, &source[0], &dest[0], 1U) == -EIO);
    CHECK(!queue.busy && queue.failed == 1U && queue.count == 0U);
    CHECK(fake.started == 0U);

    fake.start_error_once = -EAGAIN;
    CHECK(dma_queue_submit(&queue, &source[1], &dest[1], 1U) == -EAGAIN);
    CHECK(!queue.busy && queue.failed == 2U && queue.count == 0U);
    CHECK(fake.started == 0U);

    CHECK(dma_queue_submit(&queue, &source[2], &dest[2], 1U) == 0);
    CHECK(fake.started == 1U && queue.busy);

    emit(DMA_STATUS_COMPLETE);
    CHECK(queue.completed == 1U && queue.failed == 2U && !queue.busy);
    ++cases;
}

static void test_queued_start_failure_does_not_stall(void) {
    struct dma_queue queue;
    uint8_t source[3] = {1, 2, 3};
    uint8_t dest[3] = {0};
    reset();

    CHECK(dma_queue_init(&queue, &device, 0U) == 0);
    for (unsigned int i = 0U; i < 3U ; ++i) {
        CHECK(dma_queue_submit(&queue, &source[i], &dest[i], 1U) == 0);
    }
    CHECK(queue.count == 2U && fake.started == 1U);

    fake.config_error_once = -EIO;
    emit(DMA_STATUS_COMPLETE);
    CHECK(queue.failed == 1U && queue.completed == 1U);
    CHECK(queue.busy && queue.count == 0U);
    CHECK(fake.started == 2U);
    CHECK(fake.started_sources[1] == (uintptr_t)&source[2]);

    emit(DMA_STATUS_COMPLETE);
    CHECK(queue.completed == 2U && queue.failed == 1U && !queue.busy);
    ++cases;
}

int main(void) {
    test_invalid_inputs();
    test_fifo_full_and_intermediate_events();
    test_error_completion_advances_queue();
    test_synchronous_driver_failures();
    test_queued_start_failure_does_not_stall();
    printf("PASS: %lu DMA queue cases: %lu assertions \n", cases, assertions);
    puts("Host contract only; Zephyr firmware uses a simulated DMA controller");
    return EXIT_SUCCESS;
}
