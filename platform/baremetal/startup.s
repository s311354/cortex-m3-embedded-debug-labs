.syntax unified
.cpu cortex-m3
.thumb
.global Reset_Handler
.global SVC_Handler

.section .isr_vector,"a",%progbits

.word __stack_top              /* Initial MSP from linker script */

/* Cortex-M Exceptions (vector table) */
.word Reset_Handler
.word NMI_Handler
.word HardFault_Handler
.word MemManage_Handler
.word BusFault_Handler
.word UsageFault_Handler

.word 0
.word 0
.word 0
.word 0

.word SVC_Handler

.word DebugMon_Handler

.word 0

.word PendSV_Handler

.word SysTick_Handler

/* External Interrupts (IRQ0 - IRQ31) */
.word UART0_Handler            /* IRQ0: UART0 */
.word UART1_Handler            /* IRQ1: UART1 */
.word UART2_Handler            /* IRQ2: UART2 */
.word UART3_Handler            /* IRQ3: UART3 */
.word UART4_Handler            /* IRQ4: UART4 (overflow) */
.word RTC_Handler              /* IRQ5: RTC */
.word PORT0_Handler            /* IRQ6: GPIO Port 0 */
.word PORT1_Handler            /* IRQ7: GPIO Port 1 */
.word TIMER0_Handler           /* IRQ8: Timer 0 */
.word TIMER1_Handler           /* IRQ9: Timer 1 */
.word DUALTIMER_Handler        /* IRQ10: Dual Timer */
.word SPI0_Handler             /* IRQ11: SPI 0 */
.word UARTOVF_Handler          /* IRQ12: UART overflow */
.word ETHERNET_Handler         /* IRQ13: Ethernet */
.word I2S_Handler              /* IRQ14: I2S */
.word TSC_Handler              /* IRQ15: Touch Screen */
.word PORT2_Handler            /* IRQ16: GPIO Port 2 */
.word PORT3_Handler            /* IRQ17: GPIO Port 3 */
.word UART5_Handler            /* IRQ18: UART 5 */
.word UART6_Handler            /* IRQ19: UART 6 */
.word UART7_Handler            /* IRQ20: UART 7 */
.word UART8_Handler            /* IRQ21: UART 8 */
.word SPI1_Handler             /* IRQ22: SPI 1 */
.word SPI2_Handler             /* IRQ23: SPI 2 */
.word PORT0A_Handler           /* IRQ24: GPIO Port 0a */
.word PORT1A_Handler           /* IRQ25: GPIO Port 1a */
.word PORT2A_Handler           /* IRQ26: GPIO Port 2a */
.word PORT3A_Handler           /* IRQ27: GPIO Port 3a */
.word TRNG_Handler             /* IRQ28: TRNG */
.word UART9_Handler            /* IRQ29: UART 9 */
.word UART10_Handler           /* IRQ30: UART 10 */
.rept 1
.word Default_Handler          /* IRQ31: Reserved */
.endr

.text

/* Weak aliases for interrupt handlers */
.weak UART0_Handler
.weak UART1_Handler
.weak UART2_Handler
.weak UART3_Handler
.weak UART4_Handler
.weak RTC_Handler
.weak PORT0_Handler
.weak PORT1_Handler
.weak TIMER0_Handler
.weak TIMER1_Handler
.weak DUALTIMER_Handler
.weak SPI0_Handler
.weak UARTOVF_Handler
.weak ETHERNET_Handler
.weak I2S_Handler
.weak TSC_Handler
.weak PORT2_Handler
.weak PORT3_Handler
.weak UART5_Handler
.weak UART6_Handler
.weak UART7_Handler
.weak UART8_Handler
.weak SPI1_Handler
.weak SPI2_Handler
.weak PORT0A_Handler
.weak PORT1A_Handler
.weak PORT2A_Handler
.weak PORT3A_Handler
.weak TRNG_Handler
.weak UART9_Handler
.weak UART10_Handler

/* Set all weak handlers to Default_Handler by default */
.thumb_set UART0_Handler, Default_Handler
.thumb_set UART1_Handler, Default_Handler
.thumb_set UART2_Handler, Default_Handler
.thumb_set UART3_Handler, Default_Handler
.thumb_set UART4_Handler, Default_Handler
.thumb_set RTC_Handler, Default_Handler
.thumb_set PORT0_Handler, Default_Handler
.thumb_set PORT1_Handler, Default_Handler
.thumb_set TIMER0_Handler, Default_Handler
.thumb_set TIMER1_Handler, Default_Handler
.thumb_set DUALTIMER_Handler, Default_Handler
.thumb_set SPI0_Handler, Default_Handler
.thumb_set UARTOVF_Handler, Default_Handler
.thumb_set ETHERNET_Handler, Default_Handler
.thumb_set I2S_Handler, Default_Handler
.thumb_set TSC_Handler, Default_Handler
.thumb_set PORT2_Handler, Default_Handler
.thumb_set PORT3_Handler, Default_Handler
.thumb_set UART5_Handler, Default_Handler
.thumb_set UART6_Handler, Default_Handler
.thumb_set UART7_Handler, Default_Handler
.thumb_set UART8_Handler, Default_Handler
.thumb_set SPI1_Handler, Default_Handler
.thumb_set SPI2_Handler, Default_Handler
.thumb_set PORT0A_Handler, Default_Handler
.thumb_set PORT1A_Handler, Default_Handler
.thumb_set PORT2A_Handler, Default_Handler
.thumb_set PORT3A_Handler, Default_Handler
.thumb_set TRNG_Handler, Default_Handler
.thumb_set UART9_Handler, Default_Handler
.thumb_set UART10_Handler, Default_Handler

.thumb_func
Reset_Handler:
    b main

1:
    b 1b

.thumb_func
NMI_Handler:
    b .

.thumb_func
HardFault_Handler:
    b .

.thumb_func
MemManage_Handler:
    b .

.thumb_func
BusFault_Handler:
    b .

.thumb_func
UsageFault_Handler:
    b .

.thumb_func
DebugMon_Handler:
    b .

.thumb_func
PendSV_Handler:
    b .

.thumb_func
SysTick_Handler:
    b .

.thumb_func
Default_Handler:
    b .
