# Lab 00: Cross Compilation

## Overview

This lab introduces the fundamentals of **bare-metal ARM Cortex-M3 cross-compilation**. You'll learn how to build a minimal embedded application from scratch using the GNU ARM Embedded Toolchain, understand the ARMv7-M vector table structure, and execute your code on QEMU.

## Learning Objectives

- Understand cross-compilation from x86 Linux to ARM Cortex-M3
- Learn the ARMv7-M vector table structure
- Understand Cortex-M3 memory architecture
- Work with linker scripts for embedded systems
- Use the Thumb-2 instruction set
- Debug bare-metal code with QEMU and GDB

**Key Points:**
- Minimal bare-metal application with infinite loop
- `SVC_Handler` uses `naked` attribute (no function prologue/epilogue)
- No C runtime initialization in this basic lab
- Includes CMSIS device headers for platform definitions

**ARMv7-M Vector Table Structure:**
1. **Offset 0x00**: Initial Main Stack Pointer (MSP) value
2. **Offset 0x04**: Reset handler address (entry point)
3. **Offsets 0x08-0x3C**: System exceptions
4. **Offset 0x40+**: External interrupts (IRQ0-IRQ31)

**Minimal Startup Flow:**
- No `.data` section initialization
- No `.bss` section zeroing
- No C runtime initialization
- Direct branch to `main()`

**ARM Cortex-M3 Memory Map (MPS2-AN385):**
- **FLASH**: `0x0000_0000` - Code and read-only data
- **RAM**: `0x2000_0000` - Data, BSS, heap, and stack

**Section Placement:**
```
.text    → FLASH     # Code and constants
.data    → RAM       # Initialized variables (loaded from FLASH)
.bss     → RAM       # Uninitialized variables (zeroed)
.heap    → RAM       # Dynamic allocation space (2KB)
.stack   → RAM       # Function call stack (4KB)
```
### Inspect the Binary

```bash
# View disassembly
make dump

# View ELF headers and sections
arm-none-eabi-readelf -a lab00_cross_compile.elf

# View symbol table
arm-none-eabi-nm -n lab00_cross_compile.elf

# Check memory usage
arm-none-eabi-size lab00_cross_compile.elf
```

### GDB Debug Session

```gdb
# At GDB prompt
(gdb) info registers          # View CPU registers
(gdb) x/16x 0x00000000        # Examine vector table
(gdb) disassemble Reset_Handler
(gdb) break main
(gdb) continue
(gdb) info threads
(gdb) bt                      # Backtrace
```
## Key Concepts

### 1. Cross-Compilation

Cross-compilation means building code on one architecture (x86_64 Linux) to run on another (ARM Cortex-M3):

- **Host**: x86_64 Linux (your development machine)
- **Target**: ARM Cortex-M3 (embedded processor)
- **Toolchain**: `arm-none-eabi-*` (bare-metal ARM tools)

### 2. Thumb-2 Instruction Set

Cortex-M3 **only** supports Thumb-2 (no ARM mode):
- Mixed 16-bit and 32-bit instructions
- High code density
- Full 32-bit performance
- Mandatory for ARMv7-M architecture

### 3. Vector Table

The vector table is the **most critical data structure** in Cortex-M:
- Must be placed at address `0x0000_0000` (or VTOR offset)
- First word: Initial stack pointer value
- Second word: Reset handler address
- CPU loads these on reset/power-up

### 4. Exception Handlers

Exception handlers are regular C/Assembly functions:
- Placed in the vector table
- Called automatically by hardware
- Use `naked` attribute for manual stack frame control
- Stub handlers loop forever (debugging aid)

### 5. Bare-Metal Environment

"Bare-metal" means no operating system:
- No `malloc`, `printf`, file I/O (unless you implement it)
- Direct hardware access
- Full control over memory and CPU
- You provide **everything** the code needs

## Verification Steps

### 1. Verify Vector Table

```bash
arm-none-eabi-objdump -d -j .isr_vector lab00_cross_compile.elf
```

Expected output shows:
- Initial SP: `0x20010000` (top of 64KB RAM)
- Reset_Handler address
- Exception handler addresses

### 2. Verify Reset Handler

```bash
arm-none-eabi-objdump -d lab00_cross_compile.elf | grep -A5 "Reset_Handler"
```

Should show branch to `main`.

### 3. Verify Memory Sections

```bash
arm-none-eabi-size -A lab00_cross_compile.elf
```

Check that:
- `.text` is in FLASH region
- `.data`, `.bss`, `.stack`, `.heap` are in RAM region

## Resources

- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest)
- [ARMv7-M Architecture Reference Manual](https://developer.arm.com/documentation/ddi0403/latest)
- [GNU ARM Embedded Toolchain Documentation](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain)

## Summary

Lab 00 demonstrates:
✅ Cross-compilation toolchain setup  
✅ ARMv7-M vector table structure  
✅ Minimal startup code  
✅ Linker script and memory layout  
✅ Thumb-2 instruction set  
✅ QEMU emulation and GDB debugging  
