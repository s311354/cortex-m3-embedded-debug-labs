#ifndef UART_H
#define UART_H

#include <stdint.h>
#include "CM3DS_MPS2.h"
/*
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
*/

#define UART0 ((CM3DS_MPS2_UART_TypeDef*) CM3DS_MPS2_UART0_BASE)

void uart_init(void);

#endif
