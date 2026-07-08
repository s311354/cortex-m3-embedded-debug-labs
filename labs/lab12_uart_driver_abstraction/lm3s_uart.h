#ifndef UART_H
#define UART_H

#include <stdint.h>

#define UART0_BASE 0x4000C000UL

typedef struct {
    volatile uint32_t DR;
    volatile uint32_t RSR_ECR;
    uint32_t RESERVED0[4];
    volatile uint32_t FR;
    uint32_t RESERVED1;
    volatile uint32_t ILPR;
    volatile uint32_t IBRD;
    volatile uint32_t FBRD;
    volatile uint32_t LCRH;
    volatile uint32_t CTL;

    volatile uint32_t IFLS;
    volatile uint32_t IMSC;
    volatile uint32_t RIS;
    volatile uint32_t MIS;
    volatile uint32_t ICR;
} UART_TypeDef;


void uart_putc(char c);
int uart_getc(void);

#define UART0 ((UART_TypeDef*) UART0_BASE)

#endif
