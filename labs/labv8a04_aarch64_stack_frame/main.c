#include <stdint.h>
#include <arm_acle.h>

extern char __exception_vectors[];
extern char svc_resume[];

volatile uint64_t dump[4];
volatile uint64_t restored[4];

volatile uint64_t exc_return;
volatile uint64_t g_esr_el1;
volatile uint64_t g_spsr_el1;
volatile uint32_t g_result;

static const uint64_t expected[4] = {
    0x1111, 0x2222, 0x3333, 0x4444
};

__attribute__((noinline))
static void Trigger_SVC(void) {
    __asm__ volatile(
        "mov x0, #0x1111       \n"
	"mov x1, #0x2222       \n"
	"mov x2, #0x3333       \n"
        "mov x3, #0x4444       \n"

	".global svc_trigger   \n"
        "svc_trigger:          \n"
	"svc #0                \n"
	
	".global svc_resume    \n"
	"svc_resume:           \n"
	"stp x0, x1, [%0]      \n"
	"stp x2, x3, [%0, #16] \n"
	:
	: "r"(restored)
	: "x0", "x1", "x2", "x3", "memory", "cc");
}

/* Called by shared startup.S */
void SVC_Handler(const uint64_t *stack) {
    exc_return = __arm_rsr64("ELR_EL1");
    g_esr_el1 = __arm_rsr64("ESR_EL1");
    g_spsr_el1 = __arm_rsr64("SPSR_EL1");

    for (uint32_t i = 0; i < 4; ++i) {
        dump[i] = stack[i];
    }
}

int main(void) {
    g_result = 0;

    __arm_wsr64("VBAR_EL1", (uint64_t)(uintptr_t)__exception_vectors);

    __asm__ volatile(
        "isb"
	:
	:
	: "memory");

    Trigger_SVC();

    g_result = 1;

    for (uint32_t i = 0; i < 4; ++i) {
        if ((dump[i] != expected[i]) || (restored[i] != expected[i])) {
	    g_result = 2;
	}
    }

    if (exc_return != (uint64_t)(uintptr_t)svc_resume) {
        g_result = 2;
    }

    while (1) {
        __asm__ volatile("wfe");
    }
}
