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

Cortex-M3 accesses peripherals through **memory-mapped registers**. The UART peripheral exists at a fixed address in the memory map.

### Polling Pattern

**Polling** means repeatedly checking a status flag until a condition is met:

**Advantages:**
- Simple to implement
- Can provide low and predictable service latency in a dedicated tight polling loop
- Avoids interrupt entry/exit and ISR-management overhead

**Disadvantages:**
- **Wastes CPU cycles** spinning in loops
- CPU cannot do other work while waiting
- Risk of buffer overflow on RX if polling too slow
- Blocking polling can hurt CPU utilization and task schedulability in multitasking or event-driven systems

### UART Initialization Pattern

For this CMSDK UART lab, configuration follows a simple sequence:

1. **Disable** TX/RX and UART interrupt enables by clearing `CTRL`
2. Program the baud-rate divider
3. Enable TX and RX

### Baud Rate Calculation

```
BAUDDIV = Clock_Frequency / Baud_Rate
BAUDDIV = 25,000,000 / 115,200 ≈ 217
```

## Implementation Details

### uart_init()
- Disables TX/RX and UART interrupt enables while configuring
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

### 3. Inspect Disassembly

```bash
arm-none-eabi-objdump -d lab09_uart_polling.elf | less
```
Look for:
- repeated loads from the volatile `STATE` register
- big testing of the `TX_FULL` flag
- a conditional branch implementing the polling loop
- a store to the `DATA` register

## Comparison to Interrupt-Driven I/O

| Aspect | Polling (Lab 09) | Interrupts (Lab 10) |
|--------|------------------|---------------------|
| CPU utilization waiting | Busy-waits in this implementation | CPU can perform other work or sleep |
| Complexity | Lower | Higher: ISR/state/buffering |
| Service latency | Depends on polling interval/workload | Depends on interrupt latency, masking and priority |
| Typical use | Simple or short bounded waits | Asynchronous or infrequent events, concurrent work |

## Key Takeaways

✓ Cortex-M3 accesses memory-mapped peripheral registers using load/store operations 
✓ **Polling** trades CPU efficiency for implementation simplicity  
✓ **Volatile**-qualified accesses are used for MMIO registers so required hardware accesses are preserved by the compiler
✓ Configuring a peripheral in a controlled sequence helps avoid unintended behavior during reconfiguration
✓ Memory-mapped I/O places peripheral registers in the processor address map, but those accesses can have hardware side effects and do not behave like ordinary RAM 
✓ Understanding polling limitations motivates interrupt-driven designs  

## References

- ARM Cortex-M3 Technical Reference Manual
- CMSDK UART Technical Reference Manual
- ARM MPS2+ FPGA Prototyping Board Technical Reference Manual
