# Lab 13: I2C Transaction and Bit-Banging

## Overview

This lab demonstrates **software I2C (bit-banging)** implementation on the ARM Cortex-M3. Instead of using dedicated I2C hardware, the code manually controls GPIO pins to implement the I2C protocol timing and signaling. This technique is valuable when hardware I2C is unavailable, already in use, or when debugging I2C bus issues.

## Learning Objectives

- Understand I2C protocol fundamentals (START, STOP, ACK/NACK)
- Implement software-based I2C (bit-banging) using GPIO
- Learn precise timing control with software delays
- Understand memory-mapped I/O for GPIO control
- Design transaction-based peripheral driver APIs
- Use compiler attributes for optimization control
- Debug communication protocols at the bit level

## Cortex-M3 Concepts Covered

### 1. Memory-Mapped I/O (MMIO)

GPIO pins are controlled through memory-mapped peripheral registers:

```c
// Set SCL high
CM3DS_MPS2_I2C->CONTROL = CM3DS_MPS2_I2C_SCL_Msk;

// Set SCL low
CM3DS_MPS2_I2C->CONTROLC = CM3DS_MPS2_I2C_SCL_Msk;

// Read SDA state
int sda = (CM3DS_MPS2_I2C->CONTROL & CM3DS_MPS2_I2C_SDA_Msk) != 0U;
```

**Key Points**:
- Peripherals mapped to fixed addresses in ARM memory map
- Writing to `CONTROL` sets bits (output high)
- Writing to `CONTROLC` clears bits (output low)
- Reading `CONTROL` returns current pin state
- Bit masks isolate specific pins

### 2. Software Timing with Volatile

Creating precise delays without hardware timers:

```c
static void i2c_delay(void) {
    volatile uint32_t i;
    for (i = 0U; i < 100U; ++i) {
        __asm volatile ("nop");
    }
}
```

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

# Inspect GPIO state
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

### GPIO Control Registers

**CONTROL Register** (Set bits):
```c
CM3DS_MPS2_I2C->CONTROL = CM3DS_MPS2_I2C_SCL_Msk;  // SCL = 1
CM3DS_MPS2_I2C->CONTROL = CM3DS_MPS2_I2C_SDA_Msk;  // SDA = 1
```

**CONTROLC Register** (Clear bits):
```c
CM3DS_MPS2_I2C->CONTROLC = CM3DS_MPS2_I2C_SCL_Msk;  // SCL = 0
CM3DS_MPS2_I2C->CONTROLC = CM3DS_MPS2_I2C_SDA_Msk;  // SDA = 0
```

This is a common ARM peripheral pattern for atomic bit manipulation.

### Simulated ACK Mode

**Why Simulate**:
- No physical I2C device in QEMU emulation
- Allows testing protocol logic
- Set to `0` for real hardware debugging

## Software I2C Advantages

✓ **Flexible pin assignment**: Use any GPIO pins  
✓ **Multiple I2C buses**: Not limited by hardware peripherals  
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

**Prefer Hardware I2C When**:
- High-speed transfers needed
- Power efficiency critical
- CPU bandwidth limited
- Multiple concurrent peripherals

## Comparison: Software vs Hardware I2C

| Aspect | Software (Bit-Bang) | Hardware I2C |
|--------|-------------------|--------------|
| Speed | ~10-100 kHz | Up to 400 kHz (Fast Mode) |
| CPU Usage | 100% during transfer | Minimal (interrupt/DMA) |
| Flexibility | Any GPIO pins | Fixed pins |
| Timing | Software delays | Hardware timing |
| Debugging | Step through in GDB | Harder to trace |
| Multiple Buses | Limited by GPIO | Fixed by hardware |

## Key Takeaways

1. **GPIO as Protocol Interface**: Any digital protocol can be implemented in software
2. **Timing Precision**: `volatile` and inline assembly create predictable delays
3. **Protocol State Machine**: I2C requires careful sequencing of signal transitions
4. **Transaction Abstraction**: Message-based APIs separate protocol from application
5. **Compiler Control**: Attributes like `noinline` critical for timing and debugging
6. **Error Handling**: Protocol errors require clean bus state recovery
7. **Trade-offs**: Software flexibility vs hardware efficiency

## References

- [I2C-bus Specification (NXP)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [GPIO Programming on ARM](https://developer.arm.com/documentation/)
- [Linux Kernel I2C API](https://www.kernel.org/doc/html/latest/i2c/)
