# Lab 12: UART Driver Abstraction

## Overview

This lab demonstrates professional embedded driver architecture using a Hardware Abstraction Layer (HAL) pattern. You'll learn how to separate hardware-specific code from application logic using function pointers, enabling portable and maintainable embedded software design.

## Learning Objectives

- Understand driver abstraction layers in embedded systems
- Implement function pointer tables for device drivers
- Apply polymorphism patterns in C
- Recognize memory section usage (.data vs .rodata)
- Design hardware-agnostic application code
- Learn zero-cost abstraction with inline functions

## Architecture

The lab implements a three-layer architecture:

```
┌─────────────────────────────────┐
│     Application Layer           │
│        (main.c)                 │
│  Hardware-agnostic code         │
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│     Driver Layer                │
│  (uart_driver.c/h)              │
│  Abstract device interface      │
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│     Hardware Layer              │
│      (uart.c/h)                 │
│  Direct register manipulation   │
└─────────────────────────────────┘
```

### Layer Responsibilities

**Hardware Layer** (`uart.c`, `uart.h`)
- Direct memory-mapped register access
- UART peripheral initialization
- Low-level character I/O
- Hardware-specific implementation

**Driver Layer** (`uart_driver.c`, `uart_driver.h`)
- Abstract device interface using function pointers
- Device instance management
- Indirect dispatch to hardware layer

**Application Layer** (`main.c`)
- Uses UART through abstract interface
- No knowledge of hardware registers
- Portable across different UART implementations

## Key Data Structures

### Driver Operations Table

```c
struct uart_driver_ops {
    void (*init)(void);
    void (*putc)(char c);
    char (*getc)(void);
};
```

Function pointer table stored in `.rodata` (Flash memory). Enables runtime dispatch and polymorphism in C.

### Device Instance

```c
struct uart_device {
    const struct uart_driver_ops *ops;
};
```

Device instance stored in `.data` section (RAM). Points to the operations table.

### Device Registration

```c
static const struct uart_driver_ops uart_ops = {
    .init = uart_init,
    .putc = uart_putc,
    .getc = uart_getc,
};

struct uart_device uart0 = {
    .ops = &uart_ops
};
```

## Memory Layout

```
Flash (ROM):
├── .text        → Code
├── .rodata      → uart_ops (function pointer table)
└── .data (LMA)  → Initial values for uart0

RAM:
└── .data (VMA)  → uart0 (device instance)
                   Copied from Flash by startup code
```

## Expected Behavior

1. Program initializes UART through driver abstraction
2. Sends character 'A' via `uart0.ops->putc('A')`
3. Enters echo loop: reads characters and echoes them back
4. Type characters in QEMU console to see echo response

## Debugging Exercises

### 1. Inspect Function Pointer Table

```gdb
(gdb) print uart_ops
$1 = {init = 0x8000abc <uart_init>, 
      putc = 0x8000def <uart_putc>, 
      getc = 0x8000123 <uart_getc>}

(gdb) print &uart_ops
$2 = (const struct uart_driver_ops *) 0x8001234

# Verify it's in Flash (.rodata section)
(gdb) info symbol 0x8001234
uart_ops in section .rodata
```

### 2. Examine Device Instance

```gdb
(gdb) print uart0
$3 = {ops = 0x8001234 <uart_ops>}

(gdb) print &uart0
$4 = (struct uart_device *) 0x20000100

# Verify it's in RAM (.data section)
(gdb) info symbol 0x20000100
uart0 in section .data
```

### 3. Trace Indirect Function Calls

```gdb
# Break at the indirect call
(gdb) break main.c:12
Breakpoint 1 at 0x8000456: file main.c, line 12.

(gdb) continue
Breakpoint 1, main () at main.c:12
12          uart0.ops->putc('A');

# Step into the function pointer
(gdb) step
uart_putc (c=65 'A') at uart.c:8
8           while (UART0->STATE & CM3DS_MPS2_UART_STATE_TXBF_Msk) {

# Examine UART registers
(gdb) print/x *UART0
$5 = {DATA = 0x0, STATE = 0x2, CTRL = 0x7, ...}
```

### 4. Analyze Memory Sections

```bash
# View section sizes
arm-none-eabi-size lab12_uart_driver_abstraction.elf

# Examine .rodata (should contain uart_ops)
arm-none-eabi-objdump -s -j .rodata lab12_uart_driver_abstraction.elf

# Examine .data section (should contain uart0 initialization)
arm-none-eabi-objdump -s -j .data lab12_uart_driver_abstraction.elf

# See all symbols and their sections
arm-none-eabi-nm lab12_uart_driver_abstraction.elf | grep -E 'uart_ops|uart0'
```

## Real-World Applications

This pattern is used in:

- **Linux kernel**: `struct file_operations`, `struct device_driver`
- **CMSIS drivers**: Common driver interface specification
- **FreeRTOS**: Device abstraction for peripheral drivers
- **Commercial RTOS**: VxWorks, Zephyr, Azure RTOS
- **Automotive**: AUTOSAR driver architecture

## Key Takeaways

✅ Function pointers enable polymorphism in C  
✅ Proper layering improves maintainability and portability  
✅ Memory sections matter: const data in Flash, mutable in RAM  
✅ Abstraction overhead is minimal with proper design  
✅ Professional embedded code balances abstraction with efficiency  
✅ This architecture scales from microcontrollers to complex systems  

