#include <stdint.h>
#include "device.h"

volatile uint32_t g_before;
volatile uint32_t g_after_disable;
volatile uint32_t g_after_restore;

static void CriticalSection(void) {
    volatile uint32_t i;

    for (i = 0; i < 1000; ++i)
        __asm volatile ("nop");
}

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uint32_t old = __get_PRIMASK();

    g_before = old;

    __disable_irq();

    g_after_disable = __get_PRIMASK();

    CriticalSection();

    if (!old)
        __enable_irq();

    g_after_restore = __get_PRIMASK();

    while (1) {
    }
}
