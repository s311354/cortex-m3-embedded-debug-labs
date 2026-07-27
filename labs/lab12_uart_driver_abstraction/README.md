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
├── .text        → Code + const data (uart_ops merged here)
│                  Note: .rodata* merged into .text by linker script
└── .data (LMA)  → Initial values for uart0

RAM:
└── .data (VMA)  → uart0 (device instance)
                   Copied from Flash by startup code
```

**Important**: While the compiler places `const` data in `.rodata`, the linker script (`platform/runtime/linker.ld`) merges `.rodata*` into the `.text` section. This is a common embedded pattern since both code and read-only data belong in Flash. When debugging, you'll see `uart_ops` in `.text`, not as a separate `.rodata` section.

## Expected Behavior

1. Program initializes UART through driver abstraction
2. Sends character 'A' via `uart0.ops->putc('A')`
3. Enters echo loop: reads characters and echoes them back
4. Type characters in QEMU console to see echo response

## Debugging Exercises

### 1. Inspect Function Pointer Table

```gdb
(gdb) print uart_ops
$1 = {init = 0x211 <uart_init>, 
      putc = 0x235 <uart_putc>, 
      getc = 0x261 <uart_getc>}

(gdb) print &uart_ops
$2 = (const struct uart_driver_ops *) 0x284

# Verify it's in Flash (.text section)
# Note: Linker script merges .rodata* into .text section
(gdb) info symbol 0x284
uart_ops in section .text
```

### 2. Examine Device Instance

**Important**: You must examine `uart0` **after** `.data` initialization! Break at `main` to ensure the startup code has copied `.data` from Flash to RAM.

```gdb
# Break at main (after .data initialization)
(gdb) break main
(gdb) continue

# Now examine uart0
(gdb) print uart0
$3 = {ops = 0x284 <uart_ops>}

(gdb) print &uart0
$4 = (struct uart_device *) 0x20000000

# Verify it's in RAM (.data section)
(gdb) info symbol 0x20000000
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
$5 = {DATA = 0x0, STATE = 0x0, CTRL = 0xb, ...}
```

### 4. Understand .data Initialization Timing

# Now RAM has the correct value

```
(gdb) x/4xb 0x20000000
0x20000000:     0x84    0x02    0x00    0x00    ← Copied from Flash!

```

**Key insight**: Global variables with initializers are stored in Flash (`.data` LMA) and copied to RAM (`.data` VMA) by startup code before `main()`. The initial values are determined by the **linker** at build time, not computed at runtime. The startup code only performs a **memcpy operation**.

### 5. Analyze Memory Sections

```bash
# View section sizes
arm-none-eabi-size lab12_uart_driver_abstraction.elf

# Examine .text section (contains code AND const data like uart_ops)
arm-none-eabi-objdump -s -j .text lab12_uart_driver_abstraction.elf

# See where uart_ops is located (will show .text, not .rodata)
arm-none-eabi-objdump -t lab12_uart_driver_abstraction.elf | grep uart_ops
# Output: 00000284 l     O .text    0000000c uart_ops
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
✅ **Startup sequence matters**: `.data` must be copied before C code runs  
✅ **Link-time vs runtime**: Initialized globals are resolved at link time, only copied at startup  

