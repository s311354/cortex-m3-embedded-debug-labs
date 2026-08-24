.syntax unified
.cpu cortex-m3
.thumb
.global Reset_Handler

.section .isr_vector,"a",%progbits

.word __stack_top              /* Initial MSP from linker script */
.word Reset_Handler
.word NMI_Handler
.word HardFault_Handler

.rept 45
.word Default_Handler
.endr

.text

.thumb_func
Reset_Handler:
    ldr r0, =_sidata
    ldr r1, =_sdata
    ldr r2, =_edata

copy_loop:
    cmp r1, r2
    bcc copy_word
    b zero_begin

copy_word:
    ldr r3, [r0], #4
    str r3, [r1], #4
    b copy_loop

zero_begin:
    ldr r1, =_sbss
    ldr r2, =_ebss

zero_loop:
    cmp r1, r2
    bcc zero_word
    b init_done

zero_word:
    movs r3, #0
    str r3, [r1], #4
    b zero_loop

.thumb_func
init_done:
    /* initialize runtime */
    bl SystemInit
    bl __libc_init_array       /* Call libc_nano.a initialization */
    bl main
    bl __libc_fini_array       /* Call libc_nano.a cleanup */

1:
    b 1b

/*
 * _init and _fini stubs for __libc_init_array/__libc_fini_array
 * These are called by libc_nano.a before/after init_array/fini_array
 */
.thumb_func
.weak _init
_init:
    bx lr

.thumb_func
.weak _fini
_fini:
    bx lr

.thumb_func
NMI_Handler:
    b .

.thumb_func
HardFault_Handler:
    b .

.thumb_func
Default_Handler:
    b .
