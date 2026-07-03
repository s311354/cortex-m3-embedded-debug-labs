#include <stdint.h>
#include "device.h"

volatile uint32_t g_control;
volatile uint32_t g_primask;
volatile uint32_t g_ipsr;
volatile uint32_t g_msp;
volatile uint32_t g_psp;

__attribute__((naked))
void SVC_Handler(void) {

}

int main(void) {
    g_control = __get_CONTROL();

    g_primask = __get_PRIMASK();

    g_ipsr = __get_IPSR();

    g_msp = __get_MSP();

    g_psp = __get_PSP();

    while (1) {
    }
}
