#include <stdint.h>

extern uint32_t _sidata;
extern uint32_t _sdata;
extern uint32_t _edata;

extern uint32_t _sbss;
extern uint32_t _ebss;

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
