#include <stdint.h>
#include "device.h"

volatile uint32_t dump[8];
volatile uint32_t exc_return;

volatile uint32_t control_before;
volatile uint32_t control_after;

volatile uint32_t msp_before;
volatile uint32_t psp_before;

volatile uint32_t msp_after;
volatile uint32_t psp_after;

volatile uint32_t debug_control;
volatile uint32_t debug_psp;
volatile uint32_t debug_msp;

static void DumpSpecialRegisters(void) {
    __asm volatile(
        "mrs %0, control"
        : "=r"(debug_control)	
    );

    __asm volatile(
        "mrs %0, psp"
        : "=r"(debug_psp)	
    );

    __asm volatile(
        "mrs %0, msp"
        : "=r"(debug_msp)	
    );
}

/* Process Stack */
static uint32_t process_stack[64];

static void Trigger_SVC(void) {
    register uint32_t r0 asm("r0") = 0x11111111;
    register uint32_t r1 asm("r1") = 0x22222222;
    register uint32_t r2 asm("r2") = 0x33333333;
    register uint32_t r3 asm("r3") = 0x44444444;

    asm volatile("" : : "r"(r0), "r"(r1), "r"(r2), "r"(r3));

    __asm volatile ("svc #0");
}

__attribute__((naked))
void SVC_Handler(void) {
    __asm volatile(
        "tst lr,#4            \n"
	"ite eq               \n"
	"mrseq r0,msp         \n"
	"mrsne r0,psp         \n"
	"b   SVC_Handler_C    \n"
    );
}

void SVC_Handler_C(uint32_t *stack, uint32_t lr) {

    exc_return = lr;

    for (int i = 0; i < 8; ++i)
        dump[i] = stack[i];
}

int main(void) {
    uint32_t control;

    /* ---- Initial State ----*/
    control_before = __get_CONTROL();
    msp_before = __get_MSP();
    psp_before = __get_PSP();

    DumpSpecialRegisters();

    /* ---- Configure PSP ----*/
    __set_PSP((uint32_t)&process_stack[63]);

    control = __get_CONTROL();
    /* SPSEL = 1 (MSP -> PSP)
     * nPRIV = 1 (Privileged -> Unprivileged)
     * */
    control |= 0x3;

    __set_CONTROL(control);

    __ISB();

    DumpSpecialRegisters();

    control_after = __get_MSP();
    msp_after = __get_MSP();
    psp_after = __get_PSP();

    DumpSpecialRegisters();

    Trigger_SVC();

    while (1) {
    }
}
