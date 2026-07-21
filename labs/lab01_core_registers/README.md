# Lab 01: Core Registers

## Overview

This lab introduces the Cortex-M3 special purpose registers and demonstrates how to access them using CMSIS (Cortex Microcontroller Software Interface Standard) functions. Understanding these core registers is fundamental to embedded systems programming, particularly for exception handling, privilege management, and stack operations.

## Learning Objectives

- Understand Cortex-M3 special purpose registers
- Learn CMSIS Core register access functions
- Use GDB to inspect processor state
- Understand the ARMv7-M register architecture
- Debug bare-metal embedded code

## Key Concepts

### Special Purpose Registers

The lab accesses five critical Cortex-M3 registers:

#### 1. **CONTROL Register**
- **Purpose**: Controls stack pointer selection and privilege level
- **Bits**:
  - Bit 0: `nPRIV` - 0 = Privileged, 1 = Unprivileged
  - Bit 1: `SPSEL` - 0 = MSP active, 1 = PSP active
  - Bit 2: `FPCA` - Floating-point context active (Cortex-M4F only)
- **Access**: `__get_CONTROL()` / `__set_CONTROL()`

#### 2. **PRIMASK Register**
- **Purpose**: Global interrupt enable/disable
- **Bits**:
  - Bit 0: 0 = Interrupts enabled, 1 = All interrupts disabled (except NMI and HardFault)
- **Access**: `__get_PRIMASK()` / `__set_PRIMASK()`

#### 3. **IPSR (Interrupt Program Status Register)**
- **Purpose**: Indicates current exception number
- **Values**:
  - 0 = Thread mode (normal execution)
  - 1-15 = System exceptions
  - 16+ = External interrupts (IRQs)
- **Access**: `__get_IPSR()` (read-only)

#### 4. **MSP (Main Stack Pointer)**
- **Purpose**: Default stack pointer for privileged code and exception handlers
- **Usage**: OS kernel, interrupt handlers
- **Access**: `__get_MSP()` / `__set_MSP()`

#### 5. **PSP (Process Stack Pointer)**
- **Purpose**: Stack pointer for application threads
- **Usage**: User tasks in RTOS environments
- **Access**: `__get_PSP()` / `__set_PSP()`

### Why Volatile?

The `volatile` keyword prevents the compiler from optimizing away these variables, ensuring they remain in memory for GDB inspection.

### Debug with GDB

Opens GDB connected to QEMU for interactive debugging.

## Debug Session Example

```gdb
# Set breakpoint at main
(gdb) break main
Breakpoint 1 at 0x...: file main.c, line 14.

# Run to breakpoint
(gdb) continue

# Step through register reads
(gdb) next
(gdb) next

# Inspect register values
(gdb) print/x g_control
$1 = 0x0

(gdb) print/x g_primask
$2 = 0x0

(gdb) print/x g_ipsr
$3 = 0x0

(gdb) print/x g_msp
$4 = 0x20007ff0

(gdb) print/x g_psp
$5 = 0x0

# View all registers
(gdb) info registers

# Examine assembly
(gdb) disassemble main
```

## Expected Results

At startup in Thread mode:

| Register | Expected Value | Meaning |
|----------|---------------|---------|
| CONTROL  | `0x00000000` | Privileged mode, MSP active |
| PRIMASK  | `0x00000000` | Interrupts enabled |
| IPSR     | `0x00000000` | Thread mode (no exception) |
| MSP      | `0x20007FF0` | Top of RAM (depends on linker script) |
| PSP      | `0x00000000` | Not initialized yet |

## Understanding the Results

### CONTROL = 0x0
- Bit 0 (nPRIV) = 0: Running in **Privileged** mode
- Bit 1 (SPSEL) = 0: **MSP** is the active stack pointer

At reset, Cortex-M3 always starts in privileged Thread mode using MSP.

### PRIMASK = 0x0
- Interrupts are **enabled** globally

### IPSR = 0x0
- Processor is in **Thread mode** (not handling an exception)

### MSP = ~0x2000xxxx
- Points to the top of SRAM, set by the startup code from the vector table

### PSP = 0x0
- Not yet initialized since we haven't switched to using PSP

## Key Takeaways

1. **Reset State**: Cortex-M3 boots in privileged Thread mode using MSP
2. **CMSIS Abstraction**: Special registers require inline assembly, wrapped by CMSIS
3. **Dual Stack Architecture**: MSP/PSP enables separation of kernel and application stacks
4. **Exception Context**: IPSR indicates execution context (thread vs exception)
5. **Debug Methodology**: Volatile globals provide inspection points for non-printable values

## References

- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARMv7-M Architecture Reference Manual](https://developer.arm.com/documentation/ddi0403/latest/)
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/)
