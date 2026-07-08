#include <stdint.h>
#include "device.h"
#include "uart.h"

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    UART0->DR = 'A';


    while (1) {
    }
}
