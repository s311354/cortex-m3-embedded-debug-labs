#include <stdint.h>
#include "device.h"

volatile uint32_t dump[8];
volatile uint32_t exc_return;

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
        "mrs r0, msp         \n"
	"mov r1, lr           \n"
        
	"ldr r2, =exc_return \n"
	"str r1, [r2]        \n"
	"bl SVC_Handler_C    \n"
	"bx lr               \n"
    );
}

void SVC_Handler_C(uint32_t *stack) {

    for (int i = 0; i < 8; ++i)
        dump[i] = stack[i];
}

int main(void) {

    Trigger_SVC();

    while (1) {
    }
}
