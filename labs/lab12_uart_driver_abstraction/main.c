#include <stdint.h>

#include "uart_driver.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {

    // copy .data from FLASH to RAM
    uart0.ops->putc('A');

    while (1) {
    }
}
