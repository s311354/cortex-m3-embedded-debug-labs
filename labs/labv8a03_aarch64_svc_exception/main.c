#include <stdint.h>
#include <arm_acle.h>

#define ESR_EC_SVC64 (UINT64_C(0x15))

extern char __exception_vectors[];

/* ELR_EL1 is the AArch64 exception return address */
volatile uint64_t exc_return;
volatile uint64_t g_esr_el1;
volatile uint64_t g_spsr_el1;
volatile uint64_t g_vbar_el1;
volatile uint64_t g_current_el;
volatile uint64_t g_spsel;
volatile uint32_t g_svc_count;
volatile uint32_t g_after_svc;
volatile uint32_t g_result;

__attribute__((noinline))
void Trigger_SVC(void) {
    __asm__ volatile(
		    "svc #0"
		    :
		    :
		    : "memory");
}

/* Called by shared startup.S */
void SVC_Handler(const uint64_t *stack) {
    (void) stack;

    exc_return = __arm_rsr64("ELR_EL1");
    g_esr_el1 = __arm_rsr64("ESR_EL1");
    g_spsr_el1 = __arm_rsr64("SPSR_EL1");

    ++g_svc_count;
}

int main(void) {
    exc_return = 0U;
    g_esr_el1 = 0U;
    g_spsr_el1 = 0U;
    g_vbar_el1 = 0U;
    g_svc_count = 0U;
    g_after_svc = 0U;
    g_result = 0U;

    g_current_el = (__arm_rsr64("CurrentEL") >> 2U) & 0x3U;
    g_spsel = __arm_rsr64("SPSel") & 0x1U;

    /* QEMU's bare-metal */
    if ((g_current_el != 1U) || (g_spsel != 1U)) {
        g_result = 2U;
	while (1) {
	}
    }

    __arm_wsr64("VBAR_EL1", (uint64_t)(uintptr_t)__exception_vectors);

    __asm__ volatile(
		    "isb"
		    :
		    :
		    : "memory");

    g_vbar_el1 = __arm_rsr64("VBAR_EL1");

    Trigger_SVC();

    g_after_svc = 1U;
    g_result = (((g_vbar_el1 & UINT64_C(0x7FF)) == 0U) && 
		 (g_vbar_el1 == (uint64_t)(uintptr_t) __exception_vectors) &&
		 (((g_esr_el1 >> 26) & 0x3FU) == ESR_EC_SVC64) &&
		 ((g_spsr_el1 & 0xFU) == 0x5U) &&
		 (exc_return != 0U) &&
		 (g_svc_count == 1U) ? 1U : 2U);

    while (1) {
    }
}
