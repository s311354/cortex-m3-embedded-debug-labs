#include <stdint.h>
#include "uart.h"
#include "device.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uart_init();

    UART0->DATA = 'A';

    while (1) {
    }
}
