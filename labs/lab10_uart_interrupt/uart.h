#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "CM3DS_MPS2.h"

typedef struct {
    volatile uint32_t DATA;
    volatile uint32_t STATE;
    volatile uint32_t CTRL;

    union {
        volatile const uint32_t INTSTATUS;
	volatile uint32_t INTCLEAR;
    };

    volatile uint32_t BAUDDIV;
} UART_TypeDef;

void uart_init(void);
void uart_enable_irq(void);

void uart_putc(char c);
char uart_getc(void);
void uart_puts(const char *);

void UART0_Handler(void);

#define UART0 ((UART_TypeDef*) CM3DS_MPS2_UART0_BASE)

#endif
