/*
*  Minimal AArch64 startup code
*/

.section .text.boot, "ax"
.balign 4

.global _start
.type _start, %function

_start:
    /* Initialize the stack pointer (16-byte aligned) */
    ldr x0, =__stack_top
    mov sp, x0

    /* Enter the C application */
    bl main

1:
    wfe
    b 1b

.size _start, . - _start
