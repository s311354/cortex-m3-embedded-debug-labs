# Lab 08: UART Register Access

## Overview

This lab introduces **Memory-Mapped I/O (MMIO)** on the Cortex-M3 architecture by implementing direct UART register access. You'll learn how peripherals are accessed through memory addresses and how to create hardware abstractions using C structures.

## Learning Objectives

- Understand memory-mapped peripheral access on ARMv7-M
- Create hardware register definitions using C structs
- Use the `volatile` keyword correctly for hardware access
- Calculate and configure peripheral timing parameters
- Follow CMSIS naming conventions
- Perform direct register manipulation

## Target Platform

**Emulated Platform**: QEMU `mps2-an385` (ARM MPS2 + AN385, Cortex-M3)

**UART0 Peripheral**:
- Base Address: `CM3DS_MPS2_UART0_BASE` (`0x40004000`)
- Clock: 25 MHz in the QEMU `mps2-an385` model
- Target Baud Rate: ~115200 baud (BaudDiv = 217)

## Key Concepts

### Memory-Mapped I/O

On Cortex-M3, peripherals are accessed through the processor's unified 4GB address space. Writing to a peripheral register is as simple as writing to a memory location:

```c
UART0->DATA = 'A';  // Write character to UART transmit register
```
### Baud Rate Configuration

```c
UART0->BAUDDIV = 217;
```

Calculation:
```
BaudDiv = UART_Clock / Desired_Baud_Rate
        = 25,000,000 / 115,200
        ≈ 217
```

### UART Initialization

```c
void uart_init(void) {
    UART0->CTRL = 0;              // Disable TX/RX and UART interrupts while configuring
    UART0->BAUDDIV = 217;         // Set baud rate
    UART0->CTRL = 0x03;           // Enable TX, RX
}
```

**CTRL Register Bits**:
- Bit 0: Enable TX
- Bit 1: Enable RX
- Combined (0x03): Enable both transmit and receive

## Debugging Tasks

### 1. Inspect UART Register Structure

```gdb
(gdb) b main
(gdb) c
(gdb) p/x UART0
(gdb) p/x UART0->CTRL
(gdb) p/x UART0->BAUDDIV
```

### 2. Watch Register Writes

```gdb
(gdb) watch *(uint32_t*)0x40004000    # UART0 DATA register
(gdb) c
```

### 3. Examine Memory-Mapped Registers

Expected output shows the register layout:
- 0x40004004: STATE
- 0x40004008: CTRL
- 0x4000400C: INTSTATUS
- 0x40004010: BAUDDIV

### 4. Verify Initialization

```gdb
(gdb) b uart_init
(gdb) c
(gdb) n    # Step through initialization
(gdb) p/x UART0->CTRL
$1 = 0x3
```

## Expected Output
```
(QEMU console should display 'A')
```

## Key Takeaways

- Cortex-M3 uses a single 4 GB processor address map containing code, data, peripheral, and system regions
- Peripherals such as the CMSDK UART are controlled through memory-mapped registers
- C structs provide typed, named representation of a peripheral register layout
- MMIO registers should be accessed through appropriately `volatile`-qualified objects so required hardware accesses are preserved
- Cortex-M3 has no MMU-based virtual-address translation; software addresses refer to the processor memory map, subject to MPU permissions and implementation-defined aliases or remapping
- CMSIS defines standard conventions for peripheral register structures, access qualifiers, and peripheral access pointers

## Additional Resources

- [Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/)
- ARM MPS2+ AN385 Technical Reference Manual
