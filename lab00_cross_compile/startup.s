.syntax unified
.cpu cortex-m3
.thumb
.global Reset_Handler

.section .isr_vector,"a",%progbits

.word 0x20010000              /*Initial MSP*/
.word Reset_Handler
.word NMI_Handler
.word HardFault_Handler

.rept 45
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
Default_Handler:
    b .
