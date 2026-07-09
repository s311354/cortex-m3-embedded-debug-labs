#include <stdint.h>
#include "uart.h"
#include "device.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uart_init();

    uart_puts("Hello Cortex-M3\r\n");

    while (1) {
    }
}
