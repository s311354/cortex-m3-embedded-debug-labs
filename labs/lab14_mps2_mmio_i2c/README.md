# Lab 14: MPS2 MMIO I2C - Complete Driver Stack

## Overview

This lab demonstrates a **production-quality I2C driver stack** for communicating with an EEPROM device on the ARM Cortex-M3 MPS2 platform. Unlike Lab 13's generic bit-banging approach, this lab implements a complete three-layer driver architecture with hardware-specific MMIO register access, transaction-based I2C bus driver, and high-level EEPROM device abstraction.

## Learning Objectives

- Implement production I2C driver with software bit-banging using MPS2 I2C peripheral
- Master three-layer driver architecture (Application → Device → Bus → Hardware)
- Understand MMIO register patterns (SET/CLEAR registers for atomic operations)
- Design transaction-based bus APIs supporting complex multi-message operations
- Implement EEPROM device driver with page boundary management
- Use state machines for debuggable embedded applications
- Handle errors with recovery mechanisms (bus recovery, polling, timeouts)
- Support both simulation (QEMU) and real hardware with conditional compilation
- Debug multi-layer driver stacks with GDB and volatile global state

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - Test sequence state machine           │
│  - Result validation                     │
└──────────────┬──────────────────────────┘
               │ eeprom_write_byte()
               │ eeprom_read_byte()
               ▼
┌─────────────────────────────────────────┐
│  Device Driver Layer (eeprom.c)          │
│  - Memory addressing (1 or 2 byte)      │
│  - Page boundary checking                │
│  - Write polling (device ready)          │
└──────────────┬──────────────────────────┘
               │ mps2_i2c_transfer()
               │ mps2_i2c_probe()
               ▼
┌─────────────────────────────────────────┐
│  Bus Driver Layer (mps2_i2c.c)          │
│  - I2C protocol implementation           │
│  - START/STOP/RESTART conditions        │
│  - Byte read/write with ACK/NACK        │
│  - Bus recovery                          │
└──────────────┬──────────────────────────┘
               │ I2C peripheral SDA/SCL bit control
               ▼
┌─────────────────────────────────────────┐
│  Hardware Layer (MPS2 I2C Peripheral)   │
│  - CONTROL (read state / set bits)       │
│  - CONTROLC (clear bits)                 │
│  - CONTROLS (set with mask)              │
└─────────────────────────────────────────┘
```


## Cortex-M3 Concepts Covered

### 1. MPS2 I2C Peripheral MMIO Pattern

The MPS2 platform provides a minimalist I2C peripheral designed for software bit-banging. Unlike full-featured I2C hardware controllers, this peripheral only exposes SDA and SCL line control through a simple MMIO pattern:

```c
// MPS2 I2C peripheral registers (from ARM SMM_MPS2.h)
// This is a dedicated I2C peripheral, but requires software protocol implementation
struct MPS2_I2C_TypeDef {
    uint32_t CONTROL;   // Read current state, Write to set bits
    uint32_t CONTROLC;  // Write to clear bits (atomic)
    uint32_t CONTROLS;  // Write with mask to set bits (atomic)
};
```

**Why This Pattern**:
- Avoids read-modify-write hazards in multi-threaded/interrupt environments
- Each register write is a single atomic store instruction
- Common pattern in ARM peripheral design (similar to GPIO BSRR in STM32)
- No need for critical sections or interrupt disabling

This design is typical for **FPGA-based prototyping platforms** where:
- Simplified hardware reduces FPGA resource usage
- Software implementation provides protocol flexibility
- Educational value demonstrates I2C timing at the bit level
- Sufficient for low-speed configuration interfaces (audio codecs, EEPROMs)

### 2. Software Timing for I2C Protocol

Precise timing control using volatile and inline assembly:

```c
static void mps2_i2c_bit_delay(const struct mps2_i2c_bus *bus) {
    volatile uint32_t count;
    for (count = 0U; count < bus->delay_cycles; ++count)
        __asm volatile ("nop");
}
```

**Key Points**:
- `volatile` prevents compiler from optimizing away the loop
- `nop` instruction provides predictable single-cycle delay
- Configurable `delay_cycles` allows tuning for different I2C speeds
- Each function (sda_drive_low, scl_drive_low, etc.) includes delay

**I2C Timing Calculation**:
```
I2C Standard Mode: 100 kHz → 10 μs period → 5 μs per half-cycle
Cortex-M3 at 25 MHz: 25 cycles per μs → 125 cycles per half-cycle
Account for function overhead: delay_cycles = 10-100 typical
```

### 3. I2C Protocol State Machine

Complete I2C master protocol implementation:

#### START Condition

**I2C Rule**: START is SDA falling edge while SCL is high.

#### STOP Condition

**I2C Rule**: STOP is SDA rising edge while SCL is high.

#### Byte Transmission

**Protocol Details**:
- Data changes only when SCL is LOW
- Data must be stable when SCL is HIGH
- Bit 7 (MSB) transmitted first
- 9th clock cycle for slave acknowledgment

#### Clock Stretching

**I2C Clock Stretching**: Slave can hold SCL low to pause master until ready.


### 4. Transaction-Based Bus API

Modern embedded bus driver design pattern:

```c
struct mps2_i2c_msg {
    uint8_t *buf;      // Data buffer pointer
    size_t len;        // Number of bytes
    uint8_t flags;     // Transaction control flags
};

#define MPS2_I2C_MSG_WRITE    0x00U
#define MPS2_I2C_MSG_READ     0x01U
#define MPS2_I2C_MSG_STOP     0x02U
#define MPS2_I2C_MSG_RESTART  0x04U

int mps2_i2c_transfer(struct mps2_i2c_bus *bus, 
                      uint8_t target_addr,
                      struct mps2_i2c_msg *messages, 
                      size_t num_messages);
```

**Advantages**:
- Single function handles all I2C operations (write, read, combined)
- Supports multi-message transactions (RESTART between messages)
- Explicit control over START/STOP conditions
- Similar to Linux kernel `i2c_transfer()` API
- Reduces code duplication

**Bus Sequence**:
```
START → [0xA0] → ACK → [0x10] → ACK → RESTART → [0xA1] → ACK → 
[data] → NACK → STOP
```

### 5. EEPROM Device Driver Layer

High-level abstraction over I2C bus operations:

```c
struct eeprom_device {
    struct mps2_i2c_bus *bus;      // Bus this device is on
    uint8_t target_addr;            // I2C 7-bit address (0x50)
    uint8_t address_width;          // 1 or 2 byte memory addressing
    size_t page_size;               // Write page size (8 bytes)
    uint32_t ready_poll_limit;      // Polling timeout
};
```

#### Memory Addressing

**1-byte addressing** (small EEPROMs like 24C02):
```c
Write to address 0x10:
START → [0xA0] → [0x10] → [data] → STOP
         |         |        |
      Slave Addr  MemAddr  Data
```

**2-byte addressing** (large EEPROMs like 24C256):
```c
Write to address 0x1234:
START → [0xA0] → [0x12] → [0x34] → [data] → STOP
         |         |        |        |
      Slave Addr  Addr Hi  Addr Lo  Data
```

#### Page Boundary Management

EEPROMs require writes to stay within page boundaries:

**Why This Matters**:
- EEPROM page size: 8, 16, 32, or 64 bytes (device-dependent)
- Writing across page boundary causes address wrap-around
- Data written to wrong location if boundary crossed
- Driver must validate before write

**Example** (8-byte page):
```
✓ Good: Write 4 bytes at address 0x04 (stays in page 0x00-0x07)
✗ Bad:  Write 4 bytes at address 0x06 (crosses to next page)
```

### 6. State Machine Debugging Pattern

Application uses explicit state tracking for GDB inspection:

```c
enum lab14_stage {
    LAB14_STAGE_RESET = 0,
    LAB14_STAGE_BOARD_INIT,
    LAB14_STAGE_WRITE,
    LAB14_STAGE_READ,
    LAB14_STAGE_COMPARE,
    LAB14_STAGE_DONE,
    LAB14_STAGE_ERROR
};

**Why Volatile**:
- Variables inspected by GDB must be `volatile`
- Prevents compiler optimization that removes "unused" variables
- Ensures memory location exists for debugger to read
- Critical for debugging embedded state machines

**GDB Usage**:
```gdb
(gdb) break lab14_debug_checkpoint
(gdb) continue

# Inspect current state
(gdb) print g_lab14_stage
$1 = LAB14_STAGE_WRITE

(gdb) print/x g_test_write_value
$2 = 0xab

# Continue to next checkpoint
(gdb) continue

# Check result
(gdb) print g_eeprom_read_value
$3 = 0xff
```

### 7. Error Handling and Recovery

Production-quality error management:

#### Structured Error Codes
```c
enum mps2_i2c_status {
    MPS2_I2C_OK            = 0,
    MPS2_I2C_ERR_ARGUMENT  = -1,   // Invalid parameter
    MPS2_I2C_ERR_ADDRESS   = -2,   // Bad I2C address
    MPS2_I2C_ERR_NACK      = -3,   // Slave didn't ACK
    MPS2_I2C_ERR_TIMEOUT   = -4,   // Clock stretch timeout
    MPS2_I2C_ERR_BUS_BUSY  = -5    // Bus stuck
};

enum eeprom_status {
    EEPROM_OK                = 0,
    EEPROM_ERR_ARGUMENT      = -100,
    EEPROM_ERR_ADDRESS_WIDTH = -101,
    EEPROM_ERR_PAGE_BOUNDARY = -102,
    EEPROM_ERR_TRANSFER      = -103,
    EEPROM_ERR_NOT_READY     = -104 
};
```

**Benefits**:
- Separate error spaces prevent conflicts
- Descriptive names for debugging
- Negative values allow positive return values for data
- Easy to extend with new error types

#### Bus Recovery

When I2C bus is stuck (slave holding SDA low), recovery procedure:

**Why 9 Clocks**:
- I2C byte = 8 data bits + 1 ACK bit
- Slave might be mid-byte when bus hung
- 9 clocks ensures byte completes regardless of position
- STOP resets slave protocol state machine

#### Parameter Validation

Defensive programming at every layer:

**Best Practices**:
- Validate all pointer parameters against NULL
- Check size/length parameters against zero
- Validate addresses against hardware limits
- Return error before touching hardware

### 8. Simulation vs Real Hardware

Support both QEMU simulation and real hardware:

**Benefits**:
- Test driver logic without physical hardware
- Same codebase for development and production
- Easy to switch modes with compile flag
- Validates protocol implementation


### 9. Compiler Attributes for Embedded

Control code generation for debugging and timing:


**`noinline` Attribute**:
- Prevents function from being inlined into caller
- Ensures function exists in symbol table for GDB breakpoints
- Preserves timing characteristics (important for protocol timing)
- Allows profiling specific operations

**`volatile` Assembly**:
- Prevents compiler from removing "empty" function
- Creates observable side effect
- Each `nop` is exactly 1 CPU cycle on Cortex-M3

**Without `noinline`**:
```gdb
(gdb) break write_byte
Function "write_byte" not defined.  # Inlined, no function exists
```

**With `noinline`**:
```gdb
(gdb) break write_byte
Breakpoint 1 at 0x408: file mps2_i2c.c, line 234.  # Success
```

## Code Walkthrough

### I2C Write Transaction

**Operation**: Write 0xAB to EEPROM address 0x10

**Bus Sequence**:
```
START
  ↓
[0xA0]  ← Slave address (0x50 << 1 | WRITE)
  ↓
ACK     ← EEPROM acknowledges
  ↓
[0x10]  ← Memory address
  ↓
ACK     ← EEPROM acknowledges
  ↓
[0xAB]  ← Data byte
  ↓
ACK     ← EEPROM acknowledges
  ↓
STOP
```

### I2C Read Transaction (Random Read)

**Operation**: Read byte from EEPROM address 0x10

**Bus Sequence**:
```
START
  ↓
[0xA0]    ← Slave address + WRITE
  ↓
ACK
  ↓
[0x10]    ← Memory address to read from
  ↓
ACK
  ↓
RESTART   ← Repeated START (no STOP)
  ↓
[0xA1]    ← Slave address + READ (0x50 << 1 | 1)
  ↓
ACK
  ↓
[data]    ← EEPROM sends byte
  ↓
NACK      ← Master signals end of read
  ↓
STOP
```

### Advanced Debugging Techniques

#### Inspect I2C Peripheral Register State
```gdb
# View raw register values
(gdb) print/x *bus->regs
$1 = {
  CONTROL = 0x3,   # Bits: SDA=1, SCL=1 (idle)
  CONTROLC = 0x0,
  CONTROLS = 0x0
}

# Watch for register changes
(gdb) watch bus->regs->CONTROL
Hardware watchpoint 2: bus->regs->CONTROL

(gdb) continue
Hardware watchpoint 2: bus->regs->CONTROL
Old value = 0x3
New value = 0x2  # SDA driven low (START)
```

#### Trace I2C Protocol
```gdb
# Set breakpoints on all I2C primitives
(gdb) break generate_start
(gdb) break write_byte
(gdb) break read_byte
(gdb) break generate_stop

# Display current operation
(gdb) commands
> silent
> printf "I2C Operation: %s\n", __func__
> continue
> end

# See protocol flow
(gdb) run
I2C Operation: generate_start
I2C Operation: write_byte
I2C Operation: write_byte
I2C Operation: write_byte
I2C Operation: generate_stop
```

#### Examine Call Stack During Transaction
```gdb
(gdb) break write_bit
(gdb) continue

(gdb) backtrace
#0  write_bit () at mps2_i2c.c:156
#1  write_byte () at mps2_i2c.c:234
#2  send_address () at mps2_i2c.c:284
#3  mps2_i2c_transfer () at mps2_i2c.c:322
#4  eeprom_write () at eeprom.c:89
#5  eeprom_write_byte () at eeprom.c:105
#6  main () at main.c:42
```

### Using objdump for Analysis

```bash
# Disassemble entire binary
arm-none-eabi-objdump -D lab14_mps2_mmio_i2c.elf > disassembly.txt

# View specific function
arm-none-eabi-objdump -D lab14_mps2_mmio_i2c.elf | grep -A 30 "write_byte"

# Check if noinline worked
arm-none-eabi-nm lab14_mps2_mmio_i2c.elf | grep write_byte
000003f8 t write_byte  # 't' = local text symbol, function exists

# View symbol table
arm-none-eabi-nm -S lab14_mps2_mmio_i2c.elf | sort

# Inspect section sizes
arm-none-eabi-size lab14_mps2_mmio_i2c.elf
   text    data     bss     dec     hex filename
   3428      12     104    3544     dd8 lab14_mps2_mmio_i2c.elf
```

## Key Observations

### I2C Address Format

**7-bit I2C addressing** with R/W bit:

```
Byte sent on bus:  [A6][A5][A4][A3][A2][A1][A0][R/W]
                    |______ 7-bit address ______|  |
                                                   0=Write, 1=Read

EEPROM address: 0x50 (binary: 1010000)

Write operation: 0x50 << 1 | 0 = 0xA0 (10100000)
Read operation:  0x50 << 1 | 1 = 0xA1 (10100001)
```

### Clock Stretching Mechanism

**Slave controls bus timing**:

```
Master releases SCL (expects high)
         ↓
Is SCL actually high? → YES → Continue
         ↓
         NO (slave holding low)
         ↓
Wait and poll SCL → Timeout → Error
         ↓
SCL goes high (slave ready)
         ↓
Continue
```

This allows slow devices to pause the master.

### Bus Idle vs Busy Detection

**Idle Bus**: Both SDA and SCL high (both released)

**Busy Bus**: SDA low while SCL high (invalid state)
- Indicates stuck device or incomplete transaction
- Requires bus recovery procedure

## Software I2C Trade-offs

**Note**: The MPS2 I2C peripheral used in this lab is a **minimalist hardware peripheral** that still requires software bit-banging. This section compares software-based I2C (like ours) versus full-featured I2C hardware controllers (with automatic protocol handling).

### Advantages

✅ **Flexibility**: Minimal hardware allows protocol customization  
✅ **Multiple buses**: MPS2 provides multiple I2C peripheral instances  
✅ **Bus recovery**: Manual control for unsticking devices  
✅ **Custom timing**: Adapt to non-standard devices  
✅ **Debugging**: Step through protocol at bit level  
✅ **FPGA efficiency**: Simple peripheral uses fewer FPGA resources  

### Disadvantages

❌ **CPU intensive**: Wastes cycles on bit manipulation  
❌ **Interrupt sensitivity**: IRQs can break timing  
❌ **Lower speed**: Limited by software delay precision  
❌ **No DMA support**: Every byte requires CPU intervention  
❌ **Power consumption**: CPU can't sleep during transfers  
❌ **Code size**: Larger than hardware driver  

### When to Use Each

**Use Software/Minimalist I2C (like MPS2) When**:
- Full I2C hardware controller unavailable
- Low-speed configuration interfaces (audio codecs, EEPROMs)
- Need multiple I2C buses beyond hardware limit
- Non-standard timing requirements
- Bus recovery needed frequently
- Educational/debugging purposes
- FPGA resource constraints

**Use Full Hardware I2C Controller (e.g., STM32, NXP) When**:
- High-speed transfers required (>100 kHz reliable)
- Power efficiency critical
- CPU bandwidth limited
- DMA support beneficial
- Multiple peripherals competing for CPU
- Production system with proven hardware

## Key Takeaways

### 1. Driver Architecture
- **Layered design** separates concerns and improves testability
- **Device layer** abstracts protocol details from application
- **Bus layer** implements wire protocol independent of device
- **Hardware layer** provides platform-specific register access

### 2. MMIO Patterns
- **SET/CLEAR registers** enable atomic bit manipulation without RMW
- **Common in ARM** peripherals (I2C, GPIO, timers, interrupts)
- **MPS2 I2C peripheral** uses this pattern for SDA/SCL control
- **Prevents race conditions** in multi-threaded or interrupt-driven code

### 3. Protocol Implementation
- **State machines** for protocol sequencing (START, data, STOP)
- **Bit-level control** requires precise timing with volatile loops
- **Error detection** at each step (ACK/NACK, timeout, bus busy)

### 4. Embedded Debugging
- **Volatile globals** make state visible to debugger
- **noinline functions** create breakpoint targets
- **State enums** provide meaningful context in GDB
- **Checkpoint functions** mark important transitions

### 5. Production Patterns
- **Parameter validation** at API boundaries
- **Structured error codes** for actionable diagnostics  
- **Bus recovery** for fault tolerance
- **Simulation support** for testing without hardware

### 6. Compiler Control
- **`volatile`** prevents optimization of timing-critical code
- **`__attribute__((noinline))`** controls function generation
- **Inline assembly** provides cycle-accurate delays

### 7. Real-World Considerations
- **Page boundaries** matter for EEPROM writes
- **Write polling** required for non-volatile memory
- **Clock stretching** allows slow devices to control timing
- **Address width** varies by device capacity

## References

- [I2C-bus Specification and User Manual (NXP UM10204)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM MPS2 FPGA Prototyping Board Technical Reference](https://developer.arm.com/documentation/dai0386/)
- [24C02 EEPROM Datasheet (Microchip)](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0180.pdf)
- [Linux Kernel I2C Subsystem Documentation](https://www.kernel.org/doc/html/latest/i2c/)
- [Embedded Software Design Patterns](https://www.embedded.com/design-patterns-for-embedded-systems-in-c/)

