# Lab 10: UART Interrupt-Driven I/O

## Overview

This lab demonstrates **interrupt-driven peripheral I/O** on the ARM Cortex-M3. Instead of polling the UART status register (busy-waiting), the CPU is interrupted automatically when data arrives, enabling efficient multitasking and reduced power consumption.

## Learning Objectives

- Understand the Cortex-M3 Nested Vectored Interrupt Controller (NVIC)
- Configure peripheral interrupts at both hardware and NVIC levels
- Implement an Interrupt Service Routine (ISR)
- Learn proper ISR patterns: read status → service → clear flags
- Understand the hardware exception stack frame (automatic context save)
- Debug vector table and linker issues using binutils tools
- Compare interrupt-driven vs polling approaches

## Cortex-M3 Concepts Covered

### 1. NVIC (Nested Vectored Interrupt Controller)

The NVIC is the Cortex-M3's interrupt controller that manages external interrupts (IRQ0-IRQ31+).

```c
NVIC_EnableIRQ(UART0_IRQn);  // Enable IRQ0 in NVIC
```

Key points:
- External interrupts start at vector position 16 (byte offset 0x40)
- UART0 is mapped to IRQ0 (first external interrupt)
- Priority and enable/disable managed by NVIC registers

### 2. Two-Level Interrupt Enable

Cortex-M3 interrupt handling requires enabling at **two levels**:

```c
// 1. Peripheral Level - Tell UART hardware to generate interrupts
UART0->CTRL |= CM3DS_MPS2_UART_CTRL_RXIRQEN_Msk;

// 2. NVIC Level - Tell CPU to accept UART0 interrupts
NVIC_EnableIRQ(UART0_IRQn);
```

Both must be enabled for interrupts to fire.

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

- All interrupt handlers default to `Default_Handler` (infinite loop)
- Strong definitions in C code (like `uart.c`) override the weak alias
- Prevents linker errors for unimplemented handlers
- Standard practice in embedded ARM development

### 5. Hardware Exception Stack Frame

When an interrupt fires, Cortex-M3 automatically pushes to stack:
- **R0-R3**: Scratch registers
- **R12**: Intra-procedure call register
- **LR**: Return address
- **PC**: Interrupted instruction
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
❌ CPU is 100% busy checking status register  
❌ Cannot do other work while waiting  
✓ Simpler to understand and debug

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
✓ More power efficient  
❌ More complex - need proper ISR implementation

## Debugging

### Verify Handler is Linked

Check that `UART0_Handler` is present and not discarded:

```bash
arm-none-eabi-nm lab10_uart_interrupt.elf | grep UART
```

Expected output:
```
00000172 T UART0_Handler    # Strong symbol from uart.c
000001d2 W UART1_Handler    # Weak symbols → Default_Handler
...
```

### Verify Vector Table Entry

Check that UART0_Handler is in the vector table at position 16 (offset 0x40):

```bash
arm-none-eabi-objdump -s -j .isr_vector lab10_uart_interrupt.elf
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

This lab demonstrates the complete Cortex-M3 interrupt chain:

1. **Peripheral** (UART) generates interrupt signal
2. **NVIC** routes interrupt to CPU core
3. **Vector Table** directs CPU to handler function
4. **Hardware** automatically saves context
5. **ISR** services interrupt and clears flags
6. **Hardware** automatically restores context and returns
