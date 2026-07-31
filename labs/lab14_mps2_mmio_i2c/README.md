# Lab 14: MPS2 MMIO I2C - Complete Driver Stack

## Overview

This lab demonstrates a **production-quality I2C driver stack** for communicating with an EEPROM device on the ARM Cortex-M3 MPS2 platform. Unlike Lab 13's generic bit-banging approach, this lab implements a complete three-layer driver architecture with hardware-specific MMIO register access, transaction-based I2C bus driver, and high-level EEPROM device abstraction.

This represents the culmination of all previous labs, combining MMIO, protocol implementation, driver architecture, state machine debugging, and production error handling patterns into a real-world embedded system.

## Learning Objectives

- Implement production I2C driver with bit-banging using MPS2 GPIO hardware
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
               │ GPIO bit manipulation
               ▼
┌─────────────────────────────────────────┐
│  Hardware Layer (MPS2_I2C registers)    │
│  - CONTROL (set bits)                    │
│  - CONTROLC (clear bits)                 │
│  - CONTROLS (set with mask)              │
└─────────────────────────────────────────┘
```


## Cortex-M3 Concepts Covered

### 1. MPS2 GPIO MMIO Register Pattern

The MPS2 platform uses a specific MMIO pattern for atomic bit manipulation:

```c
// Hardware registers for GPIO control
struct MPS2_I2C_TypeDef {
    uint32_t CONTROL;   // Read current state, Write to set bits
    uint32_t CONTROLC;  // Write to clear bits
    uint32_t CONTROLS;  // Write with mask to set bits
};
```

**Usage Pattern**:
```c
// Set SDA high (atomic operation)
bus->regs->CONTROLS = MPS2_I2C_SDA_MASK;

// Drive SCL low (atomic operation)
bus->regs->CONTROLC = MPS2_I2C_SCL_MASK;

// Read current pin state
int scl_state = (bus->regs->CONTROL & MPS2_I2C_SCL_MASK) != 0U;
```

**Why This Pattern**:
- Avoids read-modify-write hazards in multi-threaded/interrupt environments
- Each register write is a single atomic store instruction
- Common pattern in ARM peripheral design (similar to GPIO BSRR in STM32)
- No need for critical sections or interrupt disabling

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
```c
static int generate_start(struct mps2_i2c_bus *bus) {
    sda_release(bus);           // SDA high
    wait_scl_high(bus);         // SCL high
    sda_drive_low(bus);         // SDA: high → low (START)
    scl_drive_low(bus);         // Prepare for data
    return MPS2_I2C_OK;
}
```

**I2C Rule**: START is SDA falling edge while SCL is high.

#### STOP Condition
```c
static int generate_stop(struct mps2_i2c_bus *bus) {
    sda_drive_low(bus);         // SDA low
    wait_scl_high(bus);         // SCL high
    sda_release(bus);           // SDA: low → high (STOP)
    return MPS2_I2C_OK;
}
```

**I2C Rule**: STOP is SDA rising edge while SCL is high.

#### Byte Transmission
```c
static int write_byte(struct mps2_i2c_bus *bus, uint8_t value) {
    // Send 8 data bits (MSB first)
    for (uint8_t bit = 0U; bit < 8U; ++bit) {
        write_bit(bus, (value & 0x80U) != 0U);
        value <<= 1U;
    }
    // Receive ACK on 9th clock
    return receive_ack(bus);
}
```

**Protocol Details**:
- Data changes only when SCL is LOW
- Data must be stable when SCL is HIGH
- Bit 7 (MSB) transmitted first
- 9th clock cycle for slave acknowledgment

#### Clock Stretching
```c
static int wait_scl_high(struct mps2_i2c_bus *bus) {
    scl_release(bus);  // Release SCL
    // Wait for slave to release SCL (clock stretching)
    for (uint32_t timeout = 0U; timeout < bus->timeout_cycles; ++timeout) {
        if (scl_is_high(bus) != 0) 
            return MPS2_I2C_OK;
    }
    return MPS2_I2C_ERR_TIMEOUT;
}
```

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

**Example - Simple Write**:
```c
uint8_t data[] = {0x10, 0xAB};
struct mps2_i2c_msg msg = {
    .buf = data,
    .len = 2,
    .flags = MPS2_I2C_MSG_WRITE | MPS2_I2C_MSG_STOP
};
mps2_i2c_transfer(bus, 0x50, &msg, 1);
```

**Example - Combined Write-Read** (EEPROM random read):
```c
struct mps2_i2c_msg msgs[2] = {
    {.buf = addr_buf, .len = 1, .flags = MPS2_I2C_MSG_WRITE},
    {.buf = data_buf, .len = 8, .flags = MPS2_I2C_MSG_READ | 
                                         MPS2_I2C_MSG_RESTART | 
                                         MPS2_I2C_MSG_STOP}
};
mps2_i2c_transfer(bus, 0x50, msgs, 2);
```

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

Implementation:
```c
static int build_memory_address(
    const struct eeprom_device *device,
    uint16_t memory_addr,
    uint8_t *buffer,
    size_t *address_length) {
    
    if (device->address_width == 1U) {
        buffer[0] = (uint8_t)memory_addr;
        *address_length = 1U;
    } else if (device->address_width == 2U) {
        buffer[0] = (uint8_t)(memory_addr >> 8U);   // High byte
        buffer[1] = (uint8_t)memory_addr;            // Low byte
        *address_length = 2U;
    }
}
```

#### Page Boundary Management

EEPROMs require writes to stay within page boundaries:

```c
static int write_crosses_page(
    const struct eeprom_device *device,
    uint16_t memory_addr,
    size_t length) {
    
    size_t page_offset = (size_t)memory_addr % device->page_size;
    return ((page_offset + length) > device->page_size);
}
```

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

#### Write Polling (ACK Polling)

After writing, EEPROM is busy and won't ACK until write completes:

```c
int eeprom_wait_ready(const struct eeprom_device *device) {
    for (uint32_t attempt = 0U; attempt < device->ready_poll_limit; ++attempt) {
        result = mps2_i2c_probe(device->bus, device->target_addr);
        if (result == MPS2_I2C_OK)
            return EEPROM_OK;  // Device ACKed, ready
    }
    return EEPROM_ERR_NOT_READY;  // Timeout
}
```

**I2C Probe**:
```c
int mps2_i2c_probe(struct mps2_i2c_bus* bus, uint8_t target_addr) {
    generate_start(bus);
    result = send_address(bus, target_addr, 0);  // Write address
    generate_stop(bus);
    return result;  // MPS2_I2C_OK if ACK, MPS2_I2C_ERR_NACK if busy
}
```

**Timing**: EEPROM write cycle time typically 5-10 ms.


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

volatile enum lab14_stage g_lab14_stage;
volatile int g_board_init_result;
volatile int g_eeprom_write_result;
volatile int g_eeprom_read_result;
volatile uint8_t g_test_write_value;
volatile uint8_t g_eeprom_read_value;
```

**Why Volatile**:
- Variables inspected by GDB must be `volatile`
- Prevents compiler optimization that removes "unused" variables
- Ensures memory location exists for debugger to read
- Critical for debugging embedded state machines

**Debug Checkpoints**:
```c
__attribute__((noinline))
void lab14_debug_checkpoint(void) {
    __asm volatile ("nop");
}

int main(void) {
    g_lab14_stage = LAB14_STAGE_BOARD_INIT;
    lab14_debug_checkpoint();  // Breakpoint location
    
    result = board_devices_init();
    
    if (result != MPS2_I2C_OK) {
        g_lab14_stage = LAB14_STAGE_ERROR;
        lab14_debug_checkpoint();  // Error breakpoint
    }
}
```

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

```c
int mps2_i2c_recover_bus(struct mps2_i2c_bus *bus) {
    sda_release(bus);
    
    // Send up to 9 clock pulses to unstick slave
    for (uint8_t pulse = 0U; pulse < 9U; ++pulse) {
        if (sda_is_high(bus) != 0)
            break;  // Bus recovered
        
        scl_drive_low(bus);
        wait_scl_high(bus);
    }
    
    // Send STOP to reset slave state machine
    generate_stop(bus);
    
    return (bus_is_idle(bus) != 0) ? MPS2_I2C_OK : MPS2_I2C_ERR_BUS_BUSY;
}
```

**Why 9 Clocks**:
- I2C byte = 8 data bits + 1 ACK bit
- Slave might be mid-byte when bus hung
- 9 clocks ensures byte completes regardless of position
- STOP resets slave protocol state machine

#### Parameter Validation

Defensive programming at every layer:

```c
int mps2_i2c_transfer(struct mps2_i2c_bus *bus, uint8_t target_addr, 
                      struct mps2_i2c_msg *messages, size_t num_messages) {
    // Validate bus structure
    if ((bus == NULL) || (bus->regs == NULL))
        return MPS2_I2C_ERR_ARGUMENT;
    
    // Validate messages
    if ((messages == NULL) || (num_messages == 0U))
        return MPS2_I2C_ERR_ARGUMENT;
    
    // Validate address (7-bit I2C)
    if (target_addr > 0x7FU)
        return MPS2_I2C_ERR_ADDRESS;
    
    // Validate each message buffer
    for (size_t i = 0U; i < num_messages; ++i) {
        if ((messages[i].buf == NULL) || (messages[i].len == 0U))
            return MPS2_I2C_ERR_ARGUMENT;
    }
    
    // Proceed with transfer...
}
```

**Best Practices**:
- Validate all pointer parameters against NULL
- Check size/length parameters against zero
- Validate addresses against hardware limits
- Return error before touching hardware

### 8. Simulation vs Real Hardware

Support both QEMU simulation and real hardware:

```c
struct mps2_i2c_bus {
    MPS2_I2C_TypeDef *regs;
    uint32_t delay_cycles;
    uint32_t timeout_cycles;
    uint8_t simulate_bus;  // 1 = simulation, 0 = real hardware
};
```

**Simulation Mode** (QEMU without physical EEPROM):
```c
static int scl_is_high(const struct mps2_i2c_bus *bus) {
    if (bus->simulate_bus != 0U)
        return 1;  // Simulate ideal bus behavior
    
    return ((bus->regs->CONTROL & MPS2_I2C_SCL_MASK) != 0U);
}

static int receive_ack(struct mps2_i2c_bus *bus) {
    if (bus->simulate_bus != 0U) {
        // Simulate slave ACK
        wait_scl_high(bus);
        scl_drive_low(bus);
        return MPS2_I2C_OK;
    }
    
    // Real hardware: read SDA for ACK/NACK
    result = read_bit(bus, &nack);
    return (nack == 0U) ? MPS2_I2C_OK : MPS2_I2C_ERR_NACK;
}
```

**Board Configuration**:
```c
struct mps2_i2c_bus g_shield0_i2c_bus = {
    .regs = MPS2_SHIELD0_I2C,
    .delay_cycles = 10U,
    .timeout_cycles = 10000U,
#if defined(LAB14_REAL_EEPROM)
    .simulate_bus = 0U   // Real hardware
#else
    .simulate_bus = 1U   // QEMU simulation
#endif
};
```

**Benefits**:
- Test driver logic without physical hardware
- Same codebase for development and production
- Easy to switch modes with compile flag
- Validates protocol implementation


### 9. Compiler Attributes for Embedded

Control code generation for debugging and timing:

```c
__attribute__((noinline))
void lab14_debug_checkpoint(void) {
    __asm volatile ("nop");
}

__attribute__((noinline))
static int write_byte(struct mps2_i2c_bus *bus, uint8_t value) {
    // Function body
}
```

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

**Call Stack**:
```
main()
  → eeprom_write_byte()
    → eeprom_write()
      → mps2_i2c_transfer()
        → generate_start()
        → send_address()
          → write_byte()
            → write_bit() × 8
            → receive_ack()
        → write_message()
          → write_byte()
            → write_bit() × 8
            → receive_ack()
        → generate_stop()
      → eeprom_wait_ready()
        → mps2_i2c_probe()
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

#### Inspect GPIO Register State
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

Implementation:
```c
static int send_address(struct mps2_i2c_bus *bus, 
                        uint8_t target_addr, int is_read) {
    uint8_t address_byte = (target_addr << 1U) | (is_read ? 1U : 0U);
    return write_byte(bus, address_byte);
}
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
```c
static int bus_is_idle(const struct mps2_i2c_bus *bus) {
    return (scl_is_high(bus) != 0) && (sda_is_high(bus) != 0);
}
```

**Busy Bus**: SDA low while SCL high (invalid state)
- Indicates stuck device or incomplete transaction
- Requires bus recovery procedure

## Software I2C Trade-offs

### Advantages

✅ **Flexibility**: Use any GPIO pins, not limited by hardware  
✅ **Multiple buses**: Create multiple I2C buses with different pins  
✅ **Bus recovery**: Manual control for unsticking devices  
✅ **Custom timing**: Adapt to non-standard devices  
✅ **Debugging**: Step through protocol at bit level  
✅ **No hardware conflicts**: Useful when hardware I2C unavailable  

### Disadvantages

❌ **CPU intensive**: Wastes cycles on bit manipulation  
❌ **Interrupt sensitivity**: IRQs can break timing  
❌ **Lower speed**: Limited by software delay precision  
❌ **No DMA support**: Every byte requires CPU intervention  
❌ **Power consumption**: CPU can't sleep during transfers  
❌ **Code size**: Larger than hardware driver  

### When to Use Each

**Use Software I2C When**:
- Hardware I2C peripheral unavailable or broken
- Need multiple I2C buses beyond hardware limit
- Non-standard timing requirements
- Bus recovery needed frequently
- Educational/debugging purposes
- Prototyping new I2C devices

**Use Hardware I2C When**:
- High-speed transfers required (>100 kHz reliable)
- Power efficiency critical
- CPU bandwidth limited
- DMA support beneficial
- Multiple peripherals competing for CPU
- Production system with proven hardware

### Timing Comparison

**Software I2C**:
```
Per-bit time: ~50-500 μs (depends on delay_cycles)
Max reliable speed: ~50-100 kHz
CPU utilization: 100% during transfer
```

**Hardware I2C**:
```
Per-bit time: Hardware-controlled (precise)
Max speed: 400 kHz (Fast Mode), 1 MHz (Fast Mode Plus)
CPU utilization: 1-5% (interrupt) or 0% (DMA)
```

## Key Takeaways

### 1. Driver Architecture
- **Layered design** separates concerns and improves testability
- **Device layer** abstracts protocol details from application
- **Bus layer** implements wire protocol independent of device
- **Hardware layer** provides platform-specific register access

### 2. MMIO Patterns
- **SET/CLEAR registers** enable atomic bit manipulation without RMW
- **Common in ARM** peripherals (GPIO, timers, interrupts)
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

## Hardware Setup (Real MPS2 Board)

For running on real hardware with EEPROM shield:

### Shield Configuration
```c
// In board_device.c
struct mps2_i2c_bus g_shield0_i2c_bus = {
    .regs = MPS2_SHIELD0_I2C,
    .delay_cycles = 100U,        // Tune for desired speed
    .timeout_cycles = 10000U,
    .simulate_bus = 0U           // Real hardware mode
};

const struct eeprom_device g_board_eeprom = {
    .bus = &g_shield0_i2c_bus,
    .target_addr = 0x50U,        // Standard EEPROM address
    .address_width = 1U,         // 24C02 = 1 byte, 24C256 = 2 bytes
    .page_size = 8U,             // Check EEPROM datasheet
    .ready_poll_limit = 1000U
};
```

## References

- [I2C-bus Specification and User Manual (NXP UM10204)](https://www.nxp.com/docs/en/user-guide/UM10204.pdf)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM MPS2 FPGA Prototyping Board Technical Reference](https://developer.arm.com/documentation/dai0386/)
- [24C02 EEPROM Datasheet (Microchip)](https://ww1.microchip.com/downloads/en/DeviceDoc/doc0180.pdf)
- [Linux Kernel I2C Subsystem Documentation](https://www.kernel.org/doc/html/latest/i2c/)
- [Embedded Software Design Patterns](https://www.embedded.com/design-patterns-for-embedded-systems-in-c/)

