#include <stdint.h>
#include <arm_acle.h>

volatile uint64_t g_before;
volatile uint64_t g_after_disable;
volatile uint64_t g_after_restore;

static inline void disable_irq(void) {
    __asm__ volatile (
        "msr daifset, #2"
	:
	:
	: "memory"
    );
}

static inline void enable_irq(void) {
    __asm__ volatile (
	"msr daifclr, #2"
	:
	:
	: "memory"
    );
}

#define DAIF_IRQ_MASK (UINT64_C(1) << 7)

static void CriticalSection(void) {
    volatile uint32_t i;

    for (i = 0U; i < 1000U; ++i) {
        __asm__ volatile("nop");
    }
}

int main(void) {
    uint64_t old = __arm_rsr64("DAIF");

    g_before = old;

    /* Disable IRQ: set PSTATE.I */
    disable_irq();

    g_after_disable = __arm_rsr64("DAIF");

    CriticalSection();

    /* Restore the original IRQ mask state */
    if ((old & DAIF_IRQ_MASK) == 0U) {
	enable_irq();
    }

    g_after_restore = __arm_rsr64("DAIF");

    while (1) {
    }
}
