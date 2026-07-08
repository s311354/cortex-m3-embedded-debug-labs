#include <stdint.h>

#include "uart.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    uart_init();

    uart_enable_irq();

    while (1) {
    }
}
