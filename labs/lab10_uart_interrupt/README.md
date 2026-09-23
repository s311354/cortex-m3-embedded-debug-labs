# Lab 10: UART Interrupt-Driven I/O

## Overview

This lab demonstrates interrupt-driven UART reception on the ARM Cortex-M3. Instead of busy-waiting for received data, the UART generates a RX interrupt when a character arrives. The RX path is interrupt-driven, while transmission still uses polling in `uart_putc()`.

This allows foreground code to perform other work between receive events and introduces Cortex-M3 NVIC, vector-table, and exception-handling mechanisms.

## Learning Objectives

- Understand the Cortex-M3 Nested Vectored Interrupt Controller (NVIC)
- Configure peripheral interrupts at both hardware and NVIC levels
- Implement an Interrupt Service Routine (ISR)
- Learn to identify the interrupt source, service it, and acknowledge or clear it according to the peripheral's register semantics
- Understand the hardware exception stack frame (automatic context save)
- Debug vector table and linker issues using binutils tools
- Compare interrupt-driven vs polling approaches

## Cortex-M3 Concepts Covered

### 1. NVIC (Nested Vectored Interrupt Controller)

The NVIC is integrated with the Cortex-M3 core and manages external interrupts and their enable, pending, active, and priority state.

The Cortex-M3 architecture supports an implementation-defined number of external interrupts, up to 240. QEMU `mps2-an385` configures 32 external interrupt inputs (IRQ0-IRQ31)

```c
NVIC_EnableIRQ(UART0_IRQn);  // Enable IRQ0 in NVIC
```

Key points:
- External interrupts start at vector position 16 (byte offset 0x40)
- UART0 TX is connected separately to IRQ1
- This lab enables only the UART0 RX interrupt
- Priority and enable/disable managed by NVIC registers

### 2. Two-Level Interrupt Enable

Cortex-M3 interrupt handling requires enabling at **two levels**:

```c
// 1. Peripheral Level - Tell UART hardware to generate interrupts
UART0->CTRL |= CM3DS_MPS2_UART_CTRL_RXIRQEN_Msk;

// 2. NVIC Level - Tell CPU to accept UART0 interrupts
NVIC_EnableIRQ(UART0_IRQn);
```

Both the UART interrupt source and the corresponding NVIC IRQ must be enabled. The interrupt must also not be blocked by processor-level interrupt masking such as PRIMASK/BASEPRI. This lab assumes configurable interrupts are globally unmasked.

### 3. Vector Table Mechanics

The startup code defines the vector table in `platform/baremetal/startup.s`:

```assembly
.word __stack_top           /* Position 0: Initial MSP */
.word Reset_Handler         /* Position 1: Reset */
...
.word UART0_Handler         /* Position 16: IRQ0 (offset 0x40) */
```

When UART0 asserts an interrupt:
1. Cortex-M3 hardware looks up position 16 in vector table
2. Fetches address of `UART0_Handler`
3. Automatically saves context (R0-R3, R12, LR, PC, xPSR) to stack
4. Branches to the handler in Handler mode (privileged)
5. Returns via special `EXC_RETURN` value to restore context

### 4. Weak Symbol Pattern

The startup code uses the **weak symbol pattern**:

```assembly
.weak UART0_Handler
.thumb_set UART0_Handler, Default_Handler
```
- Handlers declared with `.weak` and aliased using `.thumb_set` resolve to `Default_Handler` unless a strong definition is linked
- A strong C definition such as `UART0_Handler()` overrides the weak alias
- Prevents linker errors for unimplemented handlers
- Standard practice in embedded ARM development

### 5. Hardware Exception Stack Frame

When an interrupt fires, Cortex-M3 automatically pushes to stack:
- **R0-R3**: caller-saved registers
- **R12**: intra-procedure-call scratch register
- **LR**: link-register value from the interrupted context
- **PC**: return/resume address of the interrupted context
- **xPSR**: Processor status

This happens in hardware (no software overhead). The ISR returns via special `EXC_RETURN` value in LR.

## Interrupt-Driven vs Polling

### Polling (Lab 09)
```c
while (1) {
    char c = uart_getc();  // Busy-waits in a loop
    uart_putc(c);          // Echo back
}
```
❌Main code no longer polls the UART receive status
❌Foreground code can perform other work between receive events
✓ With `__WFI()`, the processor can sleep while waiting for interrupts

### Interrupt-Driven (Lab 10)
```c
while (1) {
    // CPU idle or doing other work
}

void UART0_Handler(void) {
    char c = UART0->DATA;
    uart_putc(c);  // Echo back
}
```
✓ CPU only active when data arrives  
✓ Can perform other tasks in main loop  
❌ More complex - need proper ISR implementation

## Debugging

### Verify Handler is Linked

Check that `UART0_Handler` is present and not discarded:

```bash
arm-none-eabi-nm lab10_uart_interrupt.elf | grep UART0_Handler
```

### Verify Vector Table Entry

Check that UART0_Handler is in the vector table at position 16 (offset 0x40):

```bash
arm-none-eabi-objdump -s -j .text lab10_uart_interrupt.elf
```
Look for offset 0x40 (IRQ0):
```
0040: 73010000 d3010000  → 0x173 points to UART0_Handler
```

(Addresses have bit 0 set for Thumb mode)

### Check Linker Map

If the handler isn't working, check the map file:

```bash
grep "UART0_Handler" lab10_uart_interrupt.map
```
Should show the handler in the `.text` section, NOT in "Discarded input sections".

## References

- ARM Cortex-M3 Technical Reference Manual
- ARMv7-M Architecture Reference Manual
- ARM MPS2+ AN385 Technical Reference Manual
- CMSIS Core Documentation

## Summary

This lab demonstrates the Cortex-M3 UART RX interrupt path:

1. The UART receives a character and asserts its enabled RX interrupt
2. The corresponding NVIC interrupt input (UART0 RX = IRQ0 on AN385) becomes pending  
3. If the IRQ is enabled and not masked, the Cortex-M3 takes the exception
4. The processor automatically stacks the basic exception frame and fetches the handler address from vector-table entry 16
5. `UART0_Handler()` reads the interrupt status, receives the byte, and acknowledges the RX interrupt through the UART `INTCLEAR` register
6. The handler echoes the character using the polling-based TX function

The RX path is interrupt-driven; the TX path remains polling-based in this lab.
