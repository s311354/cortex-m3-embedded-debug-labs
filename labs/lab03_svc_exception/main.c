#include <stdint.h>
#include "device.h"

volatile uint32_t exc_return = 0;

void Trigger_SVC(void) {
    __asm volatile ("svc #0");
}

__attribute__((naked))
void SVC_Handler(void) {
    __asm volatile(
        "mov r0, lr          \n"
	"ldr r1, =exc_return \n"
	"str r0, [r1]        \n"
	"bx lr               \n"
    );
}

int main(void) {
    Trigger_SVC();

    while (1) {
    }
}
