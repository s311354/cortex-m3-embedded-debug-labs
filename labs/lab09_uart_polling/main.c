#include <stdint.h>
#include "device.h"
#include "uart.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uart_init();

    uart_puts("Hello Cortex-M3\n");

    while (1) {
    }
}
