/*
*  ARMv8-A / AArch64 startup and EL1 exception vectors
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

/* 16 128-byte exception slots*/
.section .vectors, "ax"
.balign 2048

.global __exception_vectors
.type __exception_vectors, %object

__exception_vectors:
.macro VECTOR target
    b \target
    .balign 128
.endm
    /* Current EL, SP_EL0: 0x000 - 0x180 */
    VECTOR Default_Handler      /* 0x000 synchronous */
    VECTOR Default_Handler      /* 0x080 IRQ */
    VECTOR Default_Handler      /* 0x100 FIQ */
    VECTOR Default_Handler      /* 0x180 SError */

    /* Current EL, SP_ELx: 0x200 - 0x380 */
    VECTOR svc_sync_entry       /* 0x200 synchronous */
    VECTOR Default_Handler      /* 0x280 IRQ */
    VECTOR Default_Handler      /* 0x300 FIQ */
    VECTOR Default_Handler      /* 0x380 SError */

    /* Lower EL, AArch64: 0x400 - 0x580 */
    VECTOR lower_sync_entry     /* 0x400 synchronous */
    VECTOR Default_Handler      /* 0x480 IRQ */
    VECTOR Default_Handler      /* 0x500 FIQ */
    VECTOR Default_Handler      /* 0x580 SError */

    /* Lower EL, AArch32: 0x600 - 0x780 */
    VECTOR Default_Handler      /* 0x600 synchronous */
    VECTOR Default_Handler      /* 0x680 IRQ */
    VECTOR Default_Handler      /* 0x700 FIQ */
    VECTOR Default_Handler      /* 0x780 SError */
.size __exception_vectors, . - __exception_vectors

.section .text.exception, "ax"
.balign 4

/* EL1h -> EL0t The caller configures EL0 mappings and must not expect a return to EL1 */
.global enter_el0
.type enter_el0, %function
enter_el0:
    msr sp_el0, x1
    msr elr_el1, x0
    mov x2, #0x3c0 /* DAIF masked, EL0t */
    msr spsr_el1, x2
    isb
    eret
.size enter_el0, . - enter_el0

/* Both EL1h and lower-EL AArch64 exceptions use SP_EL1 */
.global lower_sync_entry
.type lower_sync_entry, %function
lower_sync_entry:
    b svc_sync_entry
.size lower_sync_entry, . - lower_sync_entry

.global svc_sync_entry
.type svc_sync_entry, %function
svc_sync_entry:
    /* Exception entry does not automatically save general registers. */
    sub sp,   sp, #256
    stp x0,   x1, [sp, #0]
    stp x2,   x3, [sp, #16]
    stp x4,   x5, [sp, #32]
    stp x6,   x7, [sp, #48]
    stp x8,   x9, [sp, #64]
    stp x10, x11, [sp, #80]
    stp x12, x13, [sp, #96]
    stp x14, x15, [sp, #112]
    stp x16, x17, [sp, #128]
    stp x18, x19, [sp, #144]
    stp x20, x21, [sp, #160]
    stp x22, x23, [sp, #176]
    stp x24, x25, [sp, #192]
    stp x26, x27, [sp, #208]
    stp x28, x29, [sp, #224]
    str x30,      [sp, #240]

    /* Only AArch64 SVC is handled */
    mrs x0, ESR_EL1
    lsr x0, x0, #26
    cmp x0, #0x15
    b.ne Default_Handler

    /* Pass the saved exception frame */
    mov x0, sp
    bl SVC_Handler

    ldr x30,      [sp, #240]
    ldp x28, x29, [sp, #224]
    ldp x26, x27, [sp, #208]
    ldp x24, x25, [sp, #192]
    ldp x22, x23, [sp, #176]
    ldp x20, x21, [sp, #160]
    ldp x18, x19, [sp, #144]
    ldp x16, x17, [sp, #128]
    ldp x14, x15, [sp, #112]
    ldp x12, x13, [sp, #96]
    ldp x10, x11, [sp, #80]
    ldp x8,  x9,  [sp, #64]
    ldp x6,  x7,  [sp, #48]
    ldp x4,  x5,  [sp, #32]
    ldp x2,  x3,  [sp, #16]
    ldp x0,  x1,  [sp, #0]
    add sp, sp, #256
    eret
.size svc_sync_entry, . - svc_sync_entry

.type sync_fault_entry, %function
sync_fault_entry:
    /* Pass the same software-save frame. */
    mov x0, sp
    bl Sync_Fault_Handler
    b Default_Handler
.size sync_fault_entry, . - sync_fault_entry

/* Weak fallback */
.weak Default_Handler
.type Default_Handler, %function
Default_Handler:
1:
    wfe
    b 1b
.size Default_Handler, . - Default_Handler

.weak SVC_Handler
.type SVC_Handler, %function
SVC_Handler:
    b Default_Handler
.size SVC_Handler, . - SVC_Handler

.weak Sync_Fault_Handler
.type Sync_Fault_Handler, %function
Sync_Fault_Handler:
    b Default_Handler
.size Sync_Fault_Handler, . - Sync_Fault_Handler
