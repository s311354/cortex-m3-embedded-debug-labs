# Lab 13: I2C Transaction and Bit-Banging

## Overview

This lab demonstrates a blocking, write-only software I2C transaction on a Cortex-M3 target. It operates the SCL/SDA set/clear interface at `0x4002A000` (Audio Shield 1) through a minimal I2C peripheral controller. It is a bare-metal lab, not a Zephyr application.

The MPS2/AN385 address map provides multiple I2C-compatible interfaces for audio configuration and shield expansion headers. This lab uses the AUDIOSH1 interface. The sample's address `0x50` is an illustrative EEPROM-style target address; it is not an identification of the on-board codec. The exact target part and its register protocol must be checked before using this example on hardware.

## Learning Objectives

- Understand I2C protocol fundamentals (START, STOP, ACK/NACK)
- Implement software-based I2C (bit-banging) using a minimal pin controller
- Understand software delay loops and their timing limitations
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

// This lab uses Audio Shield 1 I2C interface
#define LAB13_I2C ((CM3DS_MPS2_I2C_TypeDef *) CM3DS_MPS2_AUDIOSH1_BASE)
// Note: CM3DS_MPS2_AUDIOSH1_BASE = 0x4002A000UL
```

**Key Points**:
- This lab uses the Audio Shield 1 I2C interface at address `0x4002A000` (AUDIOSH1_BASE)
- The MPS2 platform provides multiple I2C-compatible interfaces:
  - AUDIOCFG (0x40023000) - Primary audio codec configuration
  - AUDIOSH0 (0x40029000) - Audio Shield 0
  - AUDIOSH1 (0x4002A000) - Audio Shield 1 (**used in this lab**)
- Used for I2C communication with devices on audio shield expansion connector
- Only 2 registers: `CONTROL` and `CONTROLC`
- Writing to `CONTROL` releases the corresponding SCL/SDA line toward HIGH
- Writing to `CONTROLC` actively drives the corresponding line LOW
- Reading `CONTROL` returns current pin state
- Bit 0: SCL (I2C Clock Line)
- Bit 1: SDA (I2C Data Line)
- **Not a full I2C hardware controller** - requires software protocol implementation

### 2. Software Timing with Volatile

Creating software delays without hardware timers:

**Why This Works**:
- `volatile` forces accesses to the loop counter and prevents the delay loop from being optimized away
- Inline assembly `nop` keeps an explicit no-operation instruction in each loop iteration
- Loop count contributes to the delay, but does not by itself determine the exact I2C clock speed
- Total delay also includes loop-control instructions, function calls, MMIO accesses, and execution-environment effects

**I2C Timing Requirements**:
- Standard mode: up to 100 kHz (10 μs period at 100 kHz)
- Fast mode: up to 400 kHz (2.5 μs period at 400 kHz)
- Delay must accommodate setup/hold times

### 3. Bit-Banging I2C Protocol

Manual implementation of I2C signaling conditions:

#### Byte Transmission

**I2C Protocol Rules**:
- During normal data-bit transfer, data changes when SCL is LOW
- Data remains stable when SCL is HIGH during data/ACK bits; START and STOP are exceptions
- MSB transmitted first
- 9th clock cycle for ACK/NACK

### 4. Function Attributes

**Purpose**:
- `noinline`: Prevents function inlining, useful for:
  - Setting GDB breakpoints on specific I2C protocol steps
  - Preserving visible function boundaries for debugging
  - Step-by-step protocol analysis
  - Applied to: `i2c_start()`, `i2c_restart()`, `i2c_stop()`, `i2c_send_byte()`, `i2c_receive_ack()`, `i2c_transfer_bitbang()`
- `unused`: Suppresses compiler warnings for functions that may appear unused in simple analysis
  - Applied to `i2c_sda_read()` which is used in `i2c_receive_ack()`
  - Useful when compiler optimization or static analysis doesn't recognize all call paths

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
- Supports multi-message write transactions with explicit repeated START
- Clean separation between protocol and application
- Similar to Linux kernel I2C API
- Read payload handling is not implemented; read messages return `I2C_ERR_UNSUPPORTED`

**Error Codes**:
```c
#define I2C_OK               0   // Success
#define I2C_ERR_ARGUMENT    -1   // Invalid argument (NULL pointer, zero length)
#define I2C_ERR_ADDRESS     -2   // Invalid I2C address (> 0x7F)
#define I2C_ERR_NACK        -3   // Target device did not acknowledge
#define I2C_ERR_UNSUPPORTED -4   // Unsupported operation (e.g., read)
```

**Error Handling**:
- All error paths issue `i2c_stop()` to properly release the bus
- Errors return negative values, success returns 0
- `g_i2c_result` global variable stores the transaction result for debugging

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

**Note**: The ACK/NACK response depends on whether a target device at address 0x50 responds on the I2C bus. In QEMU with the AT24C EEPROM device configured (via `QEMU_EXTRA_FLAGS`), the target will respond with ACK. Without a responding device, NACK will be detected.

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

# Inspect I2C peripheral state at Audio Shield 1 interface (0x4002A000)
(gdb) print/x ((CM3DS_MPS2_I2C_TypeDef*)0x4002A000)->CONTROL
$1 = 0x3  # Both SCL and SDA idle/HIGH

# Watch data being shifted
(gdb) break i2c_send_byte
(gdb) continue
(gdb) print/x value
$2 = 0xa0  # Address byte: 0x50 << 1 | 0 (write)

# Check result
(gdb) print g_i2c_result
$3 = 0    # I2C_OK (if EEPROM device responds with ACK)
# or
$3 = -3   # I2C_ERR_NACK (if no device at address 0x50)
```

## Key Observations

### MPS2 I2C Peripheral Registers

The peripheral has only two 32-bit registers:

**CONTROL Register** (Offset: 0x000 - Read/Write):
- **Write**: Releases the specified SCL/SDA output control bit toward HIGH
- **Read**: Returns current pin state

**CONTROLC Register** (Offset: 0x004 - Write-Only):
- **Write**: Drives the specified SCL/SDA output control bit LOW
- No software read-modify-write needed

**Why This Register Design?**

This **Set/Clear register pattern** provides simple bit manipulation:
- **Direct bit updates**: Set/clear writes avoid a software read-modify-write sequence
- **Transaction synchronization**:  Individual register writes do not make the complete I2C transaction atomic; the bus still requires a single owner at a time
- **Minimal hardware**: Reduces FPGA resource usage on MPS2 platform
- **Simple interface**: Release lines toward HIGH or drive them LOW

**Register State Table**:

| Operation | Register | Value | Bit Pattern | Result |
|-----------|----------|-------|-------------|--------|
| SCL HIGH | CONTROL | 0x01 | `0b00000001` | Release SCL |
| SDA HIGH | CONTROL | 0x02 | `0b00000010` | Release SDA |
| Both HIGH | CONTROL | 0x03 | `0b00000011` | Release SCL and SDA |
| SCL LOW | CONTROLC | 0x01 | `0b00000001` | Drive SCL LOW |
| SDA LOW | CONTROLC | 0x02 | `0b00000010` | Drive SDA LOW |
| Both LOW | CONTROLC | 0x03 | `0b00000011` | Drive SCL and SDA LOW |

### ACK/NACK Detection

The implementation reads the actual ACK/NACK from the I2C bus by sampling the SDA line during the 9th clock cycle. The `i2c_receive_ack()` function:
1. Releases SDA (allows target device to pull it LOW for ACK)
2. Raises SCL (9th clock pulse)
3. Reads SDA state (LOW = ACK, HIGH = NACK)
4. Lowers SCL

**QEMU Configuration**:
- The Makefile includes `QEMU_EXTRA_FLAGS := -device at24c-eeprom,address=0x50,rom-size=256`
- This configures a virtual AT24C EEPROM device that will respond to address 0x50
- Without this device, NACK will be returned as no target responds

### What This Peripheral Is (and Isn't)

**MPS2 I2C Interface is:**
- ✓ A minimal 2-pin controller for I2C bit-banging
- ✓ Multiple instances available (AUDIOCFG, AUDIOSH0, AUDIOSH1)
- ✓ This lab uses Audio Shield 1 interface (0x4002A000)
- ✓ Provides set/clear registers that avoid software read-modify-write for line control
- ✓ Suitable for low-speed control interfaces

**MPS2 I2C Interface is NOT:**
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
2. **Software Timing**: `volatile` and inline assembly preserve a busy-wait delay, but exact bus timing must be measured or calibrated
3. **Protocol State Machine**: I2C requires careful sequencing of signal transitions
4. **Transaction Abstraction**: Message-based APIs separate protocol from application
5. **Compiler Control**: Attributes like `noinline` are useful for debugging but do not guarantee execution timing
6. **Error Handling**: Error paths issues STOP, but full stuck-bus recovery is not implemented
7. **Trade-offs**: Software flexibility vs hardware efficiency

## References

- [I2C-bus Specification (NXP)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM Cortex-M3 DesignStart - CM3DS_MPS2.h](../ARM_M3_design/m3designstart/software/cmsis/Device/ARM/CM3DS/Include/CM3DS_MPS2.h)
- [Linux Kernel I2C API](https://www.kernel.org/doc/html/latest/i2c/)
- [CMSIS Core Documentation](https://arm-software.github.io/CMSIS_5/Core/html/index.html)
