# Lab 12: UART Driver Abstraction

## Overview

This lab demonstrates a C driver-abstraction pattern using an operations table of function pointers. Hardware-specific UART operations are exposed through a common interface, allowing higher-level console and application code to remain independent of the UART register implementation.

## Learning Objectives

- Understand driver abstraction layers in embedded systems
- Implement function pointer tables for device drivers
- Apply polymorphism patterns in C
- Recognize memory section usage (.data vs .rodata)
- Design hardware-agnostic application code
- Understand the runtime and code-size costs of inline wrappers and function-pointer dispatch

## Architecture

The lab implements a three-layer architecture:

```
┌─────────────────────────────────┐
│     Application Layer           │
│        (main.c)                 │
└─────────────────────────────────┘
              ↓
┌─────────────────────────────────┐
│     Console Layer               │
│      (console.c/h)              │
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

The application is independent of the UART register implementation as long as the console/driver interface remains compatible.

## Key Data Structures

### Driver Operations Table

```c
struct uart_driver_ops {
    void (*init)(void);
    void (*putc)(char c);
    char (*getc)(void);
};
```

The operations table provides the mechanism for C-style polymorphic dispatch; this lab binds only one concrete UART implementation.

### Device Instance

```c
struct uart_device {
    const struct uart_driver_ops *ops;
};
```

Device instance stored in `.data` section (RAM). Points to the operations table.

### Static Device Binging

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

The linker script defines two logical regions:

```
Flash (RX), starting at `0x00000000`:
├── .text        → Code + const data (uart_ops merged here)
│                  Note: .rodata* merged into .text by linker script
└── .data (LMA)  → Initial values for uart0

RAM (RWX), starting at `0x20000000`:
└── .data (VMA)  → uart0 (device instance)
                   Copied from Flash by startup code
```

The names `FLASH` and `RAM` describe the linker layout used by this lab.

## Expected Behavior

1. `console_init()` initializes UART0 through the abstract driver interface
2. `fputc('A', stdout)` sends `A` through the retargeted consol path
3. `fputc('\n', stdout)` emits `\r\n` because `retarget.c` translates newline output
4. `printf()` writes its message through the retargeted stdout path
5. The main loop blocks in `console_getc()`/ `uart_getc()` until a byte is received, then echoes that byte through `console_puts()`

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

(gdb) print &uart0

# Verify it's in RAM (.data section)
(gdb) info symbol 0x20000000

```

### 3. Trace Indirect Function Calls

```gdb
# Break at the indirect call
(gdb) break console_putc
(gdb) break uart_driver_putc
(gdb) break uart_putc
(gdb) continue
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

Similar interface/operations-table patterns appear in:

- **Linux kernel**: operation tables such as `struct file_operations`, `struct device_driver`
- **CMSIS drivers**: standardized peripheral interfaces that decouple
- **Zephyr**: generic device APIs backed by driver API structures containing function pointers

## Key Takeaways

✅ Function pointers enable polymorphism in C  
✅ Proper layering improves maintainability and portability  
✅ Memory sections matter: const data in Flash, mutable in RAM  
✅Function-pointer abstraction introduces indirect-dispatch overhead; wrapper overhead may be reduced by compiler optimization 
✅ Professional embedded code balances abstraction with efficiency  
✅ This architecture scales from microcontrollers to complex systems  
✅ **Startup sequence matters**: `.data` must be copied before C code runs  
✅ **Link-time vs runtime**: Initialized globals are resolved at link time, only copied at startup  

