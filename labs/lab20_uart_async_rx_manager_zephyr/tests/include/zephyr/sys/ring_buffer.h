#ifndef HOST_ZEPHYR_RING_BUFFER_H
#define HOST_ZEPHYR_RING_BUFFER_H

#include <stdint.h>
#include <string.h>

struct ring_buf {
    uint8_t *buffer;
    uint32_t capacity, head, tail, used;
};

static inline void ring_buf_init(struct ring_buf *rb, uint32_t size, uint8_t *storage) {
    rb->buffer = storage;
    rb->capacity = size;
    rb->head = rb->tail = rb->used = 0U;
}

static inline uint32_t ring_buf_put(struct ring_buf *rb, const uint8_t *data, uint32_t size) {
    uint32_t count = 0U;

    while (count < size && rb->used < rb->capacity) {
        rb->buffer[rb->head] = data[count++];
	rb->head = (rb->head + 1U) % rb->capacity;
	++rb->used;
    }

    return count;
}


static inline uint32_t ring_buf_get(struct ring_buf *rb, uint8_t *out, uint32_t size) {
    uint32_t count = 0U;

    while (count < size && rb->used != 0U) {
        out[count++] = rb->buffer[rb->tail];
	rb->tail = (rb->tail + 1U) % rb->capacity;
	--rb->used;
    }

    return count;
}

static inline uint32_t ring_buf_size_get(const struct ring_buf *rb) {
    return rb->used;
}

#endif
