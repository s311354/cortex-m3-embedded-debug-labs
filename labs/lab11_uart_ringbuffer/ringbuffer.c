#include "ringbuffer.h"

void rb_init(ringbuffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
}

bool rb_empty(ringbuffer_t *rb) {
    return rb->head == rb->tail;
}

bool rb_full(ringbuffer_t *rb) {
    return ((rb->head + 1) % RB_SIZE) == rb->tail;
}

bool rb_push(ringbuffer_t *rb, uint8_t c) {
    if (rb_full(rb))
        return false;

    rb->data[rb->head] = c;
    rb->head = (rb->head + 1) % RB_SIZE;

    return true;
}

bool rb_pop(ringbuffer_t *rb, uint8_t *c) {
    if (rb_empty(rb))
        return false;

    *c = rb->data[rb->tail];
    rb->tail = (rb->tail + 1) % RB_SIZE;
    return true;
}
