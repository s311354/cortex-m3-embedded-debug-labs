#include <stdint.h>
#include <arm_acle.h>

volatile uint64_t g_current_el;
volatile uint64_t g_daif;
volatile uint64_t g_spsel;
volatile uint64_t g_sp;
volatile uint64_t g_sp_el0;
volatile uint64_t g_sp_el0_valid;

int main(void) {
    uint64_t sp;

    /* Current Exception Level */
    g_current_el = (__arm_rsr64("CurrentEL") >> 2U) & 0x3U;

    /* Debug, SError, IRQ, FIQ masks */
    g_daif = __arm_rsr64("DAIF");

    /* Active stack pointer selection */
    g_spsel = __arm_rsr64("SPSel") & 0x1U;

    /* Current stack pointer */
    __asm__ volatile(
	"mov %0, sp" 
	: "=r"(sp)
    );
    g_sp = sp;

    /* SPL_EL0 is accessible from EL1 when SP_EL1 is selected */
    g_sp_el0_valid = 0U;
    g_sp_el0 = 0U;

    if ((g_current_el == 1U) && (g_spsel == 1U)) {
        g_sp_el0 = __arm_rsr64("SP_EL0");
	g_sp_el0_valid = 1U;
    }

    while (1) {
    }
}
