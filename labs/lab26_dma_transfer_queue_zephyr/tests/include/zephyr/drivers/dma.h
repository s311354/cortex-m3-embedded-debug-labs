#ifndef LAB26_HOST_DMA_H_
#define LAB26_HOST_DMA_H_

#include <stdint.h>
#include <stddef.h>
#include <zephyr/device.h>

/* Host pointers may be 64-bit */
#define CONFIG_DMA_64BIT 1

enum {
    MEMORY_TO_MEMORY = 0,
    DMA_ADDR_ADJ_INCREMENT = 0
};

enum {
    DMA_STATUS_COMPLETE = 0,
    DMA_STATUS_BLOCK = 1,
    DMA_STATUS_HALF_COMPLETE = 2
};

struct dma_block_config {
    uintptr_t source_address;
    uintptr_t dest_address;
    uint32_t block_size;
    unsigned source_addr_adj;
    unsigned dest_addr_adj;
};

typedef void (*dma_callback_t) (const struct device *, void *, uint32_t, int);

struct dma_config {
    unsigned int channel_direction;
    unsigned int source_data_size;
    unsigned int dest_data_size;
    unsigned int source_burst_length;
    unsigned int dest_burst_length;
    unsigned int block_count;
    struct dma_block_config *head_block;
    dma_callback_t dma_callback;
    void *user_data;
};

int dma_config(const struct device *, int32_t, struct dma_config *);
int dma_start(const struct device *, uint32_t);


#endif
