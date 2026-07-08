#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>

#define RB_SIZE 64

typedef struct {
    char data[RB_SIZE];

    uint32_t head;
    uint32_t tail;
} ringbuffer_t;

void rb_init(void);
void rb_push(char c);
char rb_pop(void);
int rb_empty(void);

extern ringbuffer_t rb;

#endif
