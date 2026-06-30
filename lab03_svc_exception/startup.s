.syntax unified
.cpu cortex-m3
.thumb
.global Reset_Handler
.global SVC_Handler

.section .isr_vector,"a",%progbits

.word 0x20010000              /*Initial MSP*/

/* Cortex-M Exceptions */
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

/* IRQ0 - IRQ31 */
.rept 32
.word Default_Handler
.endr

.text

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
