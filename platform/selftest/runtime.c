#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;

extern uint32_t _sbss;
extern uint32_t _ebss;

extern void (*__preinit_array_start[])(void);
extern void (*__preinit_array_end[])(void);

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

extern void (*__fini_array_start[])(void);
extern void (*__fini_array_end[])(void);

void copy_data(void) {
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;

    while (dst < &_edata) {
        *dst++ = *src++;
    }
}

void zero_bss(void) {
    uint32_t *dst = &_sbss;

    while (dst < &_edata) {
        *dst++ = 0;
    }
}

void SystemInit(void) {
}

void __local_init_array(void) {
    for (unsigned int i = 0; i < (__preinit_array_end - __preinit_array_start); ++i)
        __preinit_array_start[i]();

    for (unsigned int i = 0; i < (__init_array_end - __init_array_start); ++i)
	__init_array_start[i]();
}

/*
 * destructor runner
 */
void __local_cpp_fini(void) {
    unsigned int count;

    count = __fini_array_end - __fini_array_start;

    for (unsigned int i = count; i > 0; --i)
        __fini_array_start[i - 1]();
}
