#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>

#define RB_SIZE 64

typedef struct {
    volatile char data[RB_SIZE];

    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuffer_t;

void rb_init(ringbuffer_t *rb);
bool rb_empty(ringbuffer_t *rb);
bool rb_full(ringbuffer_t *rb);
bool rb_push(ringbuffer_t *rb,uint8_t c);
bool rb_pop(ringbuffer_t *rb, uint8_t *c);

#endif
