#include <stdint.h>

#include "device.h"
#include "farfunc.h"

extern uint8_t __far_text_load__;
extern uint8_t __far_text_start__;
extern uint8_t __far_text_end__;


__attribute__((constructor(101)))
static void relocate_far_text(void) {
    const uint8_t *src = &__far_text_load__;
    uint8_t *dst = &__far_text_start__;
    uint8_t *end = &__far_text_end__;

    while (dst < end) {
        *dst++ = *src++;
    }

    __DSB();
    __ISB();
}

/*
 * Normal caller:
 *     main()      @ FLASH ~0x0000xxx
 *
 * This function:
 *     farfunc     @ RAM   ~0x20000000
 */
__attribute__((section(".far_text"), noinline, used))
uint32_t farfunc(uint32_t value) {
    return (value ^ 0xA5A55A5AU) + 1U;
}
