# Lab 14: MPS2 MMIO I2C - Layered Driver Stack

## Overview

This lab demonstrates a **production-quality I2C driver stack** for communicating with an EEPROM device on the ARM Cortex-M3 MPS2 platform. Unlike Lab 13's generic bit-banging approach, this lab implements a complete three-layer driver architecture with hardware-specific MMIO register access, transaction-based I2C bus driver, and high-level EEPROM device abstraction. The default QEMU build connects an `at24c-eeprom` model to the Shield 1 I2C bus.

## Learning Objectives

- Implement a layered I2C driver with software bit-banding using the MPS2 I2C peripheral
- Master three-layer driver architecture (Application → Device → Bus → Hardware)
- Understand MMIO register patterns (SET/CLEAR registers for atomic operations)
- Design transaction-based bus APIs supporting write, read, and combined multi-message operations
- Implement EEPROM device driver with page boundary management
- Use state machines for debuggable embedded applications
- Handle errors with recovery mechanisms (bus recovery, polling, timeouts)
- Exercise the MMIO line interface against QEMU's modeled I2C bus and EEPROM
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
│  - CONTROL (read state, offset 0x000)    │
│  - CONTROLS (write-set alias, 0x000)     │
│  - CONTROLC (write-clear, offset 0x004)  │
└─────────────────────────────────────────┘
```


## Cortex-M3 Concepts Covered

### 1. MPS2 I2C Peripheral MMIO Pattern

The MPS2 platform provides a minimalist I2C peripheral designed for software bit-banging. Unlike full-featured I2C hardware controllers, this peripheral only exposes SDA and SCL line control through a simple MMIO pattern:

```c
// MPS2 I2C peripheral registers (from ARM SMM_MPS2.h)
// This is a dedicated I2C peripheral, but requires software protocol implementation
struct MPS2_I2C_TypeDef {
    union {
        uint32_t CONTROLS;  // Offset 0x000: write to set output bits
        uint32_t CONTROL;   // Offset 0x000: read current line state
    };
    uint32_t CONTROLC;  // Offset 0x004: write to clear output bits
};
```

**Why This Pattern**:
- Avoids software read-modify-write when setting or clearing individual output bits
- Each SET/CLEAR operation is performed with a single MMIO write
- This set/clear alias pattern is common in peripheral register interfaces
- It does not make a complete I2C transaction atomic or serialize multiple callers

This minimalist interface is useful for **FPGA-based prototyping platform** where:
- The hardware interface exposes direct SCL/SDA control instead of automatic I2C protocol generation
- Software implementation provides protocol flexibility
- Bit-level behavior remains visible for debugging and education
- The interface is suitable for low-speed configuration traffic such as audio-codex or EEPROM access

### 2. Software Timing for I2C Protocol

Software timing delay using a volatile loop and `NOP` instructions:

**Key Points**:
- `volatile` keeps the loop-counter accesses observable, but does not make the complete delay cycle-accurate
- `NOP` instructions are retained, while loop control, function calls, and MMIO accesses add additional execution time
- Configurable `delay_cycles` changes the normal software delay; actual bus frequency must be measured or calibrated
- SDA/SCL drive and release helpers call the software delay

**I2C Timing Calculation**:
```
Configured target: ~100 kHz -> 10 μs nominal period -> 5 μs nominal half-cycle
SystemCoreClock: 25 MHz -> 125 core cycles per nominal half-cycle
board_i2c_delay_cycles() divides this by BOARD_DELAY_LOOP_CYCLES(5), producing delay_cycles = 25; this is an extimate, not a cycle-accurate bus-frequency guarantee
```

### 3. I2C Protocol State Machine

Single-master I2C oprations sufficient for the lab's EEPROM write/read transactions:

#### START Condition

**I2C Rule**: START is SDA falling edge while SCL is high.

#### STOP Condition

**I2C Rule**: STOP is SDA rising edge while SCL is high.

#### Byte Transmission

**Protocol Details**:
- During normal data-bit transfer, SDA changes while SCL is LOW
- Data is stable while SCL is HIGH; START/STOP intentionally change SDA while SCL is HIGH

#### Clock Stretching

**I2C Clock Stretching**: `wait_scl_high()` releases SCL and polls the line until it becomes high or the polling limit expires.


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
- Single function handles write, read, combined transactions for this 7-bit master
- Supports multi-message transactions when each message after the first requests `RESTART`
- The first message generates START; the final message must request STOP
- Conceptually similar to Linux kernel message-based I2C transfer, but it is a lab-specific API and return convention

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
    size_t page_size;               // Write page size in bytes
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
- EEPROM page size vary by device; 8, 16, 32, and 64 bytes are common examples

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
- `volatile` gives these globals observable volatile-access semantics and helps keep state changes inspectable in memory
- It prevents the compoler from treating volatile accesses as ordinary removable accesses
- It is useful for this debug-oriented build, but GDB visibility does not generally require every inspected variable to be `volatile`

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
$3 = 0xab
```

### 7. Error Handling and Recovery

Structured error management:

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
- Separate bus-layer and EEPROM-layer error ranges make failures easier to distinguish
- Descriptive names aid debugging
- Zero represents success and negative values represent errors
- The enums can be extended with additional error types

#### Bus Recovery

When I2C bus is stuck (slave holding SDA low), recovery procedure:

**Why 9 Clocks**:
- Up to 9 recovery clocks can advance a slave that is stuck partway through a byte
- A STOP is then attempted to return the bus to an idle protocol state; recovery is not guaranteed for every fault

### 8. QEMU Device Model

The default QEMU command attaches an `at24c-eeprom` at address `0x50` to the first available I2C bus, which is Shield 1 (`0x4002A000`) on `mps2-an385`.

QEMU's AT24C model consumes two internal-address bytes for every write transaction, including when `rom-size=256`. The board descriptor therefore uses `address_width = 2`, so the byte at address `0x0010` is addressed as `0x00, 0x10`.

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
[0x00]  ← Memory address high byte
  ↓
ACK     ← EEPROM acknowledges
  ↓
[0x10]  ← Memory address low byte
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
[0x00]    ← Memory address high byte
  ↓
ACK
  ↓
[0x10]    ← Memory address low byte
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

## Key Takeaways

### 1. Driver Architecture
- **Layered design** separates concerns and improves testability
- **Device layer** abstracts protocol details from application
- **Bus layer** implements wire protocol independent of device
- **Hardware layer** provides platform-specific register access

### 2. MMIO Patterns
- **SET/CLEAR aliases** update individual output bits without a software RMW sequence
- **MPS2 I2C peripheral** uses this pattern for SDA/SCL control
- **Prevents race conditions** in multi-threaded or interrupt-driven code

### 3. Protocol Implementation
- Procedural sequencing implements START, data, ACK/NACK, RESTART, and STOP operations
- **Bit-level control** uses software delays whose actual timing must be calibrated or measured
- **Error detection** covers ACK/NACK, SCL-wait timeout, and bus-busy conditions in the applicable mode

### 4. Embedded Debugging
- **Volatile globals** keep state accesses observable and convenient to inspect in this debug build
- **noinline functions** create breakpoint targets
- **State enums** provide meaningful context in GDB
- **Checkpoint functions** mark important transitions

### 5. Production Patterns
- **Parameter validation** at API boundaries
- **Structured error codes** for actionable diagnostics  
- **Bus recovery** for fault tolerance
- **Simulation support** for control-flow testing without a physical target

## References

- [I2C-bus Specification and User Manual (NXP UM10204)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM MPS2 FPGA Prototyping Board Technical Reference](https://developer.arm.com/documentation/dai0386/)
- [24C02 EEPROM Datasheet (Microchip)](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0180.pdf)
- [Linux Kernel I2C Subsystem Documentation](https://www.kernel.org/doc/html/latest/i2c/)
- [Embedded Software Design Patterns](https://www.embedded.com/design-patterns-for-embedded-systems-in-c/)
