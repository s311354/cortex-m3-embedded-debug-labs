#include <stdint.h>
#include "device.h"

volatile uint32_t svc_flag = 0;

void Trigger_SVC(void) {
    __asm volatile ("svc #0");
}

void SVC_Handler(void) {
    svc_flag = 1;
}

int main(void) {
    Trigger_SVC();

    while (1) {
    }
}
