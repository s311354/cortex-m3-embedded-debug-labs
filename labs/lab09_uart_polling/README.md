# Lab 09: UART Polling

## Overview

This lab demonstrates **polling-based UART communication** on the Cortex-M3 processor. You'll learn how to implement a simple UART driver that uses busy-waiting to transmit and receive data, understanding the trade-offs of this fundamental I/O pattern.

## Learning Objectives

- Understand **memory-mapped I/O (MMIO)** for peripheral access
- Implement **polling-based** (busy-wait) I/O
- Configure UART peripheral registers (control, baud rate, status)
- Use **volatile** keyword for hardware register access
- Understand bit manipulation for hardware control
- Learn peripheral initialization sequences
- Recognize the limitations of polling I/O

## Key Concepts

### Memory-Mapped I/O

Cortex-M3 accesses peripherals through **memory-mapped registers**. The UART peripheral exists at a fixed address in the memory map:

```c
#define UART0 ((UART_TypeDef*) CM3DS_MPS2_UART0_BASE)
```

Reading/writing to this address actually accesses hardware registers, not RAM.

### UART Register Structure

```c
typedef struct {
    volatile uint32_t DATA;       // TX/RX data register
    volatile uint32_t STATE;      // Status flags (TX_FULL, RX_FULL)
    volatile uint32_t CTRL;       // Control register (enable TX/RX)
    union {
        volatile const uint32_t INTSTATUS;  // Read: interrupt status
        volatile uint32_t INTCLEAR;          // Write: clear interrupts
    };
    volatile uint32_t BAUDDIV;    // Baud rate divider
} UART_TypeDef;
```

The `volatile` qualifier prevents compiler optimizations that could break hardware interactions.

### Polling Pattern

**Polling** means repeatedly checking a status flag until a condition is met:

**Advantages:**
- Simple to implement
- Deterministic timing (no interrupt latency)
- No context switching overhead

**Disadvantages:**
- **Wastes CPU cycles** spinning in loops
- CPU cannot do other work while waiting
- Risk of buffer overflow on RX if polling too slow
- Not suitable for real-time systems with multiple tasks

### Peripheral Initialization

Standard sequence for configuring Cortex-M3 peripherals:

1. **Disable** peripheral (safe state)
2. **Configure** settings (baud rate, mode)
3. **Enable** peripheral with desired features

### Baud Rate Calculation

```
BAUDDIV = Clock_Frequency / Baud_Rate
BAUDDIV = 25,000,000 / 115,200 ≈ 217
```

## Implementation Details

### uart_init()
- Resets UART to known state
- Sets baud rate divider
- Enables transmitter and receiver

### uart_putc()
- Polls `STATE.TX_FULL` bit until TX buffer has space
- Writes character to `DATA` register

### uart_puts()
- Sends null-terminated string
- Calls `uart_putc()` for each character

### uart_getc()
- Polls `STATE.RX_FULL` bit until data available
- Reads character from `DATA` register

## Debugging Exercises

### 1. Examine UART Registers

Set breakpoint after `uart_init()`:

```gdb
(gdb) b main.c:11
(gdb) c
(gdb) p/x *(UART_TypeDef*)0x40004000
```

**Expected:**
- `CTRL = 0x3` (TX_EN | RX_EN)
- `BAUDDIV = 217`

### 2. Watch Polling Loop

Set breakpoint in `uart_putc()`:

```gdb
(gdb) b uart.c:17
(gdb) c
(gdb) si
```

Step through the `while` loop and observe how it spins until `STATE.TX_FULL` clears.

### 3. Measure Polling Overhead

Count instructions executed in polling loop:

```gdb
(gdb) b uart_putc
(gdb) commands
> silent
> set $count = 0
> c
> end
(gdb) b uart.c:18
(gdb) commands
> silent
> set $count = $count + 1
> c
> end
(gdb) c
(gdb) p $count
```

This shows how many times the CPU checked the status bit—all wasted cycles.

### 4. Inspect Disassembly

```bash
arm-none-eabi-objdump -d lab09_uart_polling.elf | less
```

Look at `uart_putc`:
- The `while` loop compiles to a branch instruction
- See the load-store operations for MMIO access

## Comparison to Interrupt-Driven I/O

| Aspect | Polling (Lab 09) | Interrupts (Lab 10) |
|--------|------------------|---------------------|
| CPU Efficiency | Low (busy-waiting) | High (CPU freed while waiting) |
| Complexity | Simple | More complex (ISR, buffers) |
| Responsiveness | Depends on poll rate | Immediate (hardware-driven) |
| Best For | Single-task, simple I/O | Multi-tasking, async events |

## Key Takeaways

✓ Cortex-M3 uses **load-store architecture** for all I/O  
✓ **Polling** trades CPU efficiency for implementation simplicity  
✓ **Volatile** keyword is essential for hardware register access  
✓ Proper peripheral initialization prevents hardware glitches  
✓ Memory-mapped I/O makes peripherals look like regular memory  
✓ Understanding polling limitations motivates interrupt-driven designs  

## References

- ARM Cortex-M3 Technical Reference Manual
- CMSDK UART Technical Reference Manual
- ARM MPS2+ FPGA Prototyping Board Technical Reference Manual
