# Lab 13: I2C Transaction and Bit-Banging

## Overview

This lab demonstrates **software I2C (bit-banging)** implementation on the ARM Cortex-M3. Instead of using a full-featured I2C hardware controller, the code manually controls the I2C bus lines (SCL and SDA) to implement the I2C protocol timing and signaling in software. This technique is valuable when full I2C hardware is unavailable, already in use, or when debugging I2C bus issues.

The MPS2 platform provides a minimal I2C peripheral (`CM3DS_MPS2_I2C`) that exposes only basic pin control through two registers. The peripheral has two dedicated pins (SCL and SDA) for I2C communication with the on-board audio codec. Unlike full-featured I2C hardware controllers found in production microcontrollers, this peripheral provides no automatic protocol generation - all I2C logic (START/STOP conditions, byte transmission, ACK/NACK) must be implemented in software.

## Learning Objectives

- Understand I2C protocol fundamentals (START, STOP, ACK/NACK)
- Implement software-based I2C (bit-banging) using a minimal pin controller
- Learn precise timing control with software delays
- Understand memory-mapped I/O for peripheral control
- Design transaction-based peripheral driver APIs
- Use compiler attributes for optimization control
- Debug communication protocols at the bit level

## Cortex-M3 Concepts Covered

### 1. Memory-Mapped I/O (MMIO)

The MPS2 I2C peripheral provides a minimal register interface with direct pin control for software I2C bit-banging:

```c
// MPS2 I2C peripheral structure (from CM3DS_MPS2.h)
typedef struct {
    __IO uint32_t  CONTROL;   // Offset: 0x000 - Read state / Write to set bits
    __IO uint32_t  CONTROLC;  // Offset: 0x004 - Write to clear bits (atomic)
} CM3DS_MPS2_I2C_TypeDef;

// Bit mask definitions
#define CM3DS_MPS2_I2C_SCL_Pos  0
#define CM3DS_MPS2_I2C_SCL_Msk  (1UL << CM3DS_MPS2_I2C_SCL_Pos)  // 0x00000001

#define CM3DS_MPS2_I2C_SDA_Pos  1
#define CM3DS_MPS2_I2C_SDA_Msk  (1UL << CM3DS_MPS2_I2C_SDA_Pos)  // 0x00000002

// Peripheral instance mapped to Audio Configuration base address
#define CM3DS_MPS2_I2C  ((CM3DS_MPS2_I2C_TypeDef *) 0x40023000UL)
```

**Key Points**:
- Peripheral mapped to address `0x40023000` (AUDIOCFG_BASE)
- Used for I2C communication with audio codec on MPS2 board
- Only 2 registers: `CONTROL` and `CONTROLC`
- Writing to `CONTROL` sets bits HIGH (outputs 1)
- Writing to `CONTROLC` clears bits LOW (outputs 0) - atomic operation
- Reading `CONTROL` returns current pin state
- Bit 0: SCL (I2C Clock Line)
- Bit 1: SDA (I2C Data Line)
- **Not a full I2C hardware controller** - requires software protocol implementation

### 2. Software Timing with Volatile

Creating precise delays without hardware timers:

**Why This Works**:
- `volatile` prevents compiler optimization of the loop
- Inline assembly `nop` creates predictable CPU cycles
- Loop count determines I2C clock speed
- Each `nop` = 1 CPU cycle on Cortex-M3

**I2C Timing Requirements**:
- Standard mode: 100 kHz (10 μs period)
- Fast mode: 400 kHz (2.5 μs period)
- Delay must accommodate setup/hold times

### 3. Bit-Banging I2C Protocol

Manual implementation of I2C signaling conditions:

#### Byte Transmission

**I2C Protocol Rules**:
- Data changes only when SCL is LOW
- Data stable when SCL is HIGH
- MSB transmitted first
- 9th clock cycle for ACK/NACK

### 4. Function Attributes

**Purpose**:
- `noinline`: Prevents function inlining, useful for:
  - Setting GDB breakpoints
  - Ensuring predictable timing
  - Profiling specific operations
- `unused`: Suppresses warnings for conditionally-used functions
  - Functions controlled by `#if` macros
  - Debug-only helpers

### 5. Transaction-Based API Design

Structured message-based communication:

```c
struct i2c_msg {
    uint8_t *buf;      // Data buffer
    uint32_t len;      // Number of bytes
    uint8_t flags;     // Transaction control flags
};
```

**Advantages**:
- Supports complex multi-message transactions
- Clean separation between protocol and application
- Similar to Linux kernel I2C API
- Enables combined read/write operations

### 6. I2C Addressing

**I2C Address Format**:
```
Bit: [7] [6] [5] [4] [3] [2] [1] [0]
     [A6][A5][A4][A3][A2][A1][A0][R/W]
```
- Bits 7-1: 7-bit device address
- Bit 0: 0 = Write, 1 = Read

## Application Example

**I2C Bus Sequence**:
```
START → [0xA0] → ACK → [0x10] → ACK → [0xAB] → ACK → STOP
         |             |             |
      Address+W     Reg Addr      Data Value
```

## Debug Session

```gdb
# Set breakpoints
(gdb) break main
(gdb) break i2c_start
(gdb) break i2c_send_byte

# Run
(gdb) continue

# Step through I2C transaction
(gdb) next
(gdb) step

# Inspect I2C peripheral state
(gdb) print/x CM3DS_MPS2_I2C->CONTROL
$1 = 0x3

# Watch data being shifted
(gdb) break i2c_send_byte
(gdb) continue
(gdb) print/x value
$2 = 0xa0

# Check result
(gdb) print g_i2c_result
$3 = 0    # I2C_OK
```

## Key Observations

### MPS2 I2C Peripheral Registers

The peripheral has only two 32-bit registers:

**CONTROL Register** (Offset: 0x000 - Read/Write):
- **Write**: Sets specified bits HIGH (logic 1)
- **Read**: Returns current pin state

**CONTROLC Register** (Offset: 0x004 - Write-Only):
- **Write**: Clears specified bits LOW (logic 0)
- Atomic operation - no read-modify-write needed

**Why This Register Design?**

This **Set/Clear register pattern** provides simple, atomic bit manipulation:
- **Atomic operations**: Single instruction, no race conditions
- **Interrupt-safe**: No need for critical sections or read-modify-write
- **Minimal hardware**: Reduces FPGA resource usage on MPS2 platform
- **Simple interface**: Just set bits HIGH or clear bits LOW

**Register State Table**:

| Operation | Register | Value | Bit Pattern | Result |
|-----------|----------|-------|-------------|--------|
| SCL HIGH | CONTROL | 0x01 | `0b00000001` | SCL=1 |
| SDA HIGH | CONTROL | 0x02 | `0b00000010` | SDA=1 |
| Both HIGH | CONTROL | 0x03 | `0b00000011` | SCL=1, SDA=1 |
| SCL LOW | CONTROLC | 0x01 | `0b00000001` | SCL=0 |
| SDA LOW | CONTROLC | 0x02 | `0b00000010` | SDA=0 |
| Both LOW | CONTROLC | 0x03 | `0b00000011` | SCL=0, SDA=0 |

### Simulated ACK Mode

**Why Simulate**:
- No physical I2C device in QEMU emulation
- Allows testing protocol logic
- Set to `0` for real hardware debugging

### What This Peripheral Is (and Isn't)

**CM3DS_MPS2_I2C is:**
- ✓ A minimal 2-pin controller for I2C bit-banging
- ✓ Mapped to audio configuration address space (0x40023000)
- ✓ Used for configuring the audio codec on MPS2 board
- ✓ Provides atomic set/clear operations
- ✓ Suitable for low-speed control interfaces

**CM3DS_MPS2_I2C is NOT:**
- ❌ A full I2C hardware controller
- ❌ Capable of automatic protocol generation
- ❌ Equipped with shift registers or ACK detection
- ❌ Able to generate interrupts
- ❌ Compatible with DMA
- ❌ General-purpose GPIO (pins are dedicated for I2C)

This design is typical for **FPGA-based prototyping platforms** where simplified hardware reduces resource usage and provides educational value by exposing protocol implementation details.

## Software I2C Advantages

✓ **Flexible implementation**: Can adapt protocol timing and behavior  
✓ **Multiple I2C buses**: Not limited by hardware peripheral count  
✓ **Bus recovery**: Can manipulate clock to recover stuck devices  
✓ **Custom timing**: Adapt to non-standard devices  
✓ **Debug visibility**: Step through protocol at bit level

## Software I2C Disadvantages

❌ **CPU intensive**: Wastes cycles on bit manipulation  
❌ **Timing sensitive**: Affected by interrupts and code changes  
❌ **Lower speed**: Limited by software delay precision  
❌ **No DMA**: Every byte requires CPU intervention  
❌ **Power consumption**: CPU cannot sleep during transfers

## When to Use Bit-Banging

**Good Use Cases**:
- Prototyping and debugging I2C devices
- Limited hardware I2C controllers
- Non-standard I2C timing requirements
- I2C bus recovery and diagnostics
- Educational purposes

## Key Takeaways

1. **Minimal Peripheral Interface**: MPS2_I2C provides only basic pin control, not full I2C hardware
2. **Timing Precision**: `volatile` and inline assembly create predictable delays
3. **Protocol State Machine**: I2C requires careful sequencing of signal transitions
4. **Transaction Abstraction**: Message-based APIs separate protocol from application
5. **Compiler Control**: Attributes like `noinline` critical for timing and debugging
6. **Error Handling**: Protocol errors require clean bus state recovery
7. **Trade-offs**: Software flexibility vs hardware efficiency

## References

- [I2C-bus Specification (NXP)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM Cortex-M3 DesignStart - CM3DS_MPS2.h](../ARM_M3_design/m3designstart/software/cmsis/Device/ARM/CM3DS/Include/CM3DS_MPS2.h)
- [Linux Kernel I2C API](https://www.kernel.org/doc/html/latest/i2c/)
- [CMSIS Core Documentation](https://arm-software.github.io/CMSIS_5/Core/html/index.html)
