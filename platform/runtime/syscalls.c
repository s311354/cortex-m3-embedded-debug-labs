#include <sys/types.h>
#include <errno.h>

extern char __heap_start__;
extern char __heap_end__;

static char *heap_ptr = &__heap_start__;

caddr_t _sbrk(int incr) {
    char *prev_heap_ptr = heap_ptr;

    // check for overflow
    if (heap_ptr + incr > &__heap_end__) {
        errno = ENOMEM;
	return (caddr_t) -1;
    }

    // check for stack collision
    register char *sp asm("sp");

    if (heap_ptr + incr > sp) {
        errno = ENOMEM;
	return (caddr_t) -1;
    }

    heap_ptr += incr;
    return (caddr_t) prev_heap_ptr;
}
