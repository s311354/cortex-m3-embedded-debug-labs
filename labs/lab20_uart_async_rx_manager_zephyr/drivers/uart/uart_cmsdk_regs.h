/*
 * Lab-local register view based on Zephyr drivers/serial/uart_cmsdk_apb.c
 */
#ifndef LAB20_UART_CMSDK_REGS_H_
#define LAB20_UART_CMSDK_REGS_H_

#include <stdint.h>

struct uart_cmsdk_regs {
    volatile uint32_t data;
    volatile uint32_t state;
    volatile uint32_t ctrl;
    union {
        volatile const uint32_t intstatus;
	volatile uint32_t intclear;
    };
    volatile uint32_t bauddiv;
};

/* DATA */
#define UART_CMSDK_DATA_MASK         0xffU

/* CTRL */
#define UART_CMSDK_CTRL_TXEN         (1U << 0)
#define UART_CMSDK_CTRL_RXEN         (1U << 1)
#define UART_CMSDK_CTRL_TXIRQEN      (1U << 2)
#define UART_CMSDK_CTRL_RXIRQEN      (1U << 3)
#define UART_CMSDK_CTRL_TXORIRQEN    (1U << 4)
#define UART_CMSDK_CTRL_RXORIRQEN    (1U << 5)
#define UART_CMSDK_CTRL_HSTM         (1U << 6)

/* STATE */
#define UART_CMSDK_STATE_TXBF        (1U << 0)
#define UART_CMSDK_STATE_RXBF        (1U << 1)
#define UART_CMSDK_STATE_TXOR        (1U << 2)
#define UART_CMSDK_STATE_RXOR        (1U << 3)

/* INSTATUS / INTCLEAR */
#define UART_CMSDK_INT_TX            (1U << 0)
#define UART_CMSDK_INT_RX            (1U << 1)
#define UART_CMSDK_INT_TXOR          (1U << 2)
#define UART_CMSDK_INT_RXOR          (1U << 3)

/* BAUDDIV */
#define UART_CMSDK_BAUDDIV_MASK      0xfffffU
#define UART_CMSDK_BAUDDIV_MIN       16U

#endif


