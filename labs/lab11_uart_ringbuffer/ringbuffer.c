#include "ringbuffer.h"

ringbuffer_t rb;

void rb_init(void) {
    rb.head = 0;
    rb.tail = 0;
}

void rb_push(char c) {
    uint32_t next;

    next = (rb.head + 1) % RB_SIZE;

    if (next != rb.tail) {
        rb.data[rb.head] = c;
	rb.head = next;
    }
}

char rb_pop(void) {
    char c;

    c = rb.data[rb.tail];
    ++rb.tail;
    rb.tail %= RB_SIZE;
    
    return c;
}

int rb_empty(void) {
    return rb.head == rb.tail;
}
