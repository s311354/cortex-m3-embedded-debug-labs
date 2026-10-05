# Lab 21: I2C Register Update with Zephyr RTOS

## Overview

This lab demonstrates **production-quality I2C register access patterns** using the Zephyr RTOS on ARM Cortex-M3 (MPS2/AN385). It implements three fundamental register manipulation operations—read, write, and update—with a focus on the **read-modify-write pattern** commonly used in embedded driver development. The lab validates these operations against a QEMU-emulated AT24C EEPROM device configured through Zephyr's Device Tree system.

Unlike earlier bare-metal I2C labs (Lab 13, Lab 14), this lab leverages Zephyr's I2C subsystem, demonstrating the transition from low-level hardware control to RTOS-based driver abstraction while maintaining visibility into the underlying Cortex-M3 hardware.

## Learning Objectives

- Implement standard I2C register access patterns (read, write, update)
- Master the read-modify-write operation for bitfield manipulation
- Use Zephyr's Device Tree for compile-time hardware configuration
- Understand RTOS-based I2C driver architecture vs. bare-metal approaches
- Design testable embedded firmware with state machines and volatile globals
- Apply redundant-write suppression for bus optimization
- Debug RTOS applications with GDB-visible state tracking
- Validate hardware abstraction layers through systematic testing
- Exercise Zephyr I2C API (`i2c_write_read`, `i2c_write`)
- Configure QEMU device emulation through Zephyr project configuration

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - State machine with 12 test stages     │
│  - Volatile globals for GDB visibility   │
│  - Systematic validation of operations   │
└──────────────┬──────────────────────────┘
               │ reg_read_byte()
               │ reg_write_byte()
               │ reg_update_byte()
               ▼
┌─────────────────────────────────────────┐
│  Register Access Layer (reg_access.c)    │
│  - Read: single register read            │
│  - Write: single register write          │
│  - Update: read-modify-write with mask   │
│  - Input validation                      │
└──────────────┬──────────────────────────┘
               │ i2c_write_read()
               │ i2c_write()
               ▼
┌─────────────────────────────────────────┐
│  Zephyr I2C Subsystem                    │
│  - Device driver framework               │
│  - Bus locking and error handling        │
│  - Platform-independent I2C API          │
└──────────────┬──────────────────────────┘
               │ I2C controller hardware access
               ▼
┌─────────────────────────────────────────┐
│  MPS2 I2C Hardware (0x4002A000)          │
│  - I2C Shield 1 controller               │
│  - MMIO registers (CONTROL/CONTROLC)     │
│  - Software bit-banging via driver       │
└──────────────┬──────────────────────────┘
               │ I2C bus protocol (SCL/SDA)
               ▼
┌─────────────────────────────────────────┐
│  QEMU AT24C EEPROM Emulation             │
│  - Address: 0x2a (7-bit)                 │
│  - 256-byte storage                      │
│  - Writable configuration registers      │
└─────────────────────────────────────────┘
```

## Cortex-M3 Concepts Covered

### 1. Zephyr Device Tree Configuration

Device Tree provides compile-time hardware configuration, eliminating magic numbers and enabling portable driver code:

**Device Tree Overlay (app.overlay)**:
```dts
&i2c_shield1 {
    status = "okay";
    clock-frequency = <100000>;  // 100 kHz I2C standard mode

    sensor0: sensor@2a {
        compatible = "lab21,sensor";
        reg = <0x2a>;             // 7-bit I2C address
        status = "okay";

        lab21,test-reg = <0x10>;   // Register address for testing
        lab21,test-mask = <0x0f>;  // Bit mask for update operation
        lab21,test-value = <0x05>; // New value for masked bits
    };
};
```

**C Code Device Tree Access**:
```c
#define SENSOR_NODE DT_NODELABEL(sensor0)
#define I2C_NODE    DT_NODELABEL(i2c_shield1)

#define TEST_REG    ((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_reg))
#define TEST_MASK   ((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_mask))
#define TEST_VALUE  ((uint8_t)DT_PROP(SENSOR_NODE, lab21_test_value))
```

**Custom Device Tree Binding (dts/bindings/sensor/lab21,sensor.yaml)**:
```yaml
description: Lab21 generic sensor with 8-bit I2C configuration registers
compatible: "lab21,sensor"
include: i2c-device.yaml

properties:
  lab21,test-reg:
    type: int
    required: true
    description: Safe readable/writable 8-bit configuration register

  lab21,test-mask:
    type: int
    required: true
    description: Configuration bits changed by the interview exercise

  lab21,test-value:
    type: int
    required: true
    description: Value supplying the new masked bits
```

**Key Points**:
- Device Tree separates hardware configuration from driver logic
- `DT_*` macros expand to constants at compile time (zero runtime overhead)
- Custom bindings define application-specific properties
- `BUILD_ASSERT()` catches configuration errors at compile time
- I2C bus device pointer obtained via `DEVICE_DT_GET(DT_BUS(SENSOR_NODE))`
- Compatible with Cortex-M3's memory-mapped peripheral architecture

### 2. MPS2 I2C Peripheral Memory Map

The I2C controller is accessed through MMIO at a fixed Cortex-M3 address:

```c
// From application
g_i2c_mmio_base = (uintptr_t) DT_REG_ADDR(I2C_NODE);
// On MPS2/AN385: 0x4002A000 (Shield 1 I2C interface)
```

**Memory Map Context**:
```
Cortex-M3 Address Space (ARMv7-M):
0x00000000 - 0x0007FFFF: Flash (512 KB)
0x20000000 - 0x2001FFFF: SRAM (128 KB)
0x40000000 - 0x5FFFFFFF: Peripheral region
  0x4002A000: I2C Shield 1 ← This lab
  ...
```

**Key Points**:
- Cortex-M3 has no MMU—addresses are physical
- Peripherals live in dedicated memory region (0x40000000+)
- I2C registers accessed via standard load/store instructions
- Zephyr driver abstracts register-level details
- Lab exposes base address in `g_i2c_mmio_base` for GDB inspection

### 3. I2C Client Structure

Minimal abstraction for I2C device access:

```c
struct i2c_client {
    const struct device *bus;  // Zephyr I2C controller device
    uint16_t addr;             // 7-bit I2C target address
};

static const struct i2c_client g_sensor = {
    .bus = DEVICE_DT_GET(DT_BUS(SENSOR_NODE)),
    .addr = DT_REG_ADDR(SENSOR_NODE),  // 0x2a
};
```

**Key Points**:
- Separates bus controller from target device address
- Supports multiple devices on same bus
- Uses standard errno-style error codes
- Device readiness checked before operations
- Similar to Linux kernel `struct i2c_client`

### 4. Register Access Operations

Three fundamental patterns for I2C register manipulation:

#### Read Byte

**I2C Bus Sequence**:
```
START → [0x54] → ACK → [0x10] → ACK → RESTART →
        (addr+W)       (reg)
→ [0x55] → ACK → [data] ← ACK ← STOP
  (addr+R)       (value)
```

#### Write Byte

**I2C Bus Sequence**:
```
START → [0x54] → ACK → [0x10] → ACK → [0xAB] → ACK → STOP
        (addr+W)       (reg)          (value)
```

#### Update Byte (Read-Modify-Write)

**Example**:
```c
// Register 0x10 contains: 0b10110011 (0xB3)
// Mask:                    0b00001111 (0x0F) - update lower 4 bits
// Value:                   0b00000101 (0x05) - new bits

// Calculation:
old_value = 0b10110011;
new_value = (0b10110011 & ~0b00001111) | (0b00000101 & 0b00001111)
         = (0b10110011 &  0b11110000) |  0b00000101
         =  0b10110000 | 0b00000101
         =  0b10110101  (0xB5)
```

**Key Points**:
- Atomic from caller's perspective (within single-threaded context)
- Preserves unmasked bits
- Redundant-write suppression reduces bus traffic
- Common pattern in sensor/peripheral configuration
- Not hardware-atomic (requires bus locking in multi-threaded RTOS)

### 5. QEMU Device Emulation Configuration

The lab uses QEMU's AT24C EEPROM model as the I2C target device:

**Project Configuration (prj.conf)**:
```ini
# QEMU device emulation
CONFIG_QEMU_EXTRA_FLAGS="-device at24c-eeprom,id=lab21_regs,bus=/versatile_i2c/i2c,address=0x2a,rom-size=256,address-size=1,writable=on"
```

**QEMU Device Parameters**:
- `at24c-eeprom`: EEPROM device model
- `address=0x2a`: 7-bit I2C address (matches Device Tree)
- `rom-size=256`: 256 bytes of storage
- `address-size=1`: 8-bit internal addressing
- `writable=on`: Allow register writes

**Key Points**:
- QEMU emulation enables I2C testing without hardware
- Device configuration must match Device Tree settings
- Writable storage allows register update validation
- Zephyr's I2C driver talks to QEMU's device model
- Same code runs on real hardware with appropriate board configuration

## Test Sequence

The lab executes a comprehensive validation sequence:

### 1. Bus Initialization
- Validate I2C controller readiness
- Capture hardware addresses for GDB inspection

### 2. Read Original Value
- Test `reg_read_byte()` implementation
- Establish baseline register state
- Save for final restoration

### 3. Direct Write Test
- Calculate test value: `original ^ mask`
- Write using `reg_write_byte()`
- Read back and verify
- Validates basic write functionality

### 4. State Restoration
- Restore original value
- Ensures clean state for update test

### 5. Update Operation Test
- Calculate expected value: `(original & ~mask) | (value & mask)`
- Execute `reg_update_byte()`
- Read back and verify
- Validates read-modify-write logic

### 6. Redundant-Write Suppression Test
- Call `reg_update_byte()` with current value
- Should return success without I2C transaction
- Validates optimization logic

### 7. Final Restoration
- Restore hardware to original state
- Good embedded practice: leave hardware unchanged

## Key Differences from Bare-Metal Labs

| Aspect | Bare-Metal (Lab 13/14) | Zephyr RTOS (Lab 21) |
|--------|------------------------|----------------------|
| **I2C Driver** | Custom bit-banging implementation | Zephyr I2C subsystem |
| **Configuration** | Hardcoded addresses/constants | Device Tree |
| **Timing** | Manual delay loops | Driver handles timing |
| **Portability** | MPS2-specific | Device Tree enables portability |
| **Error Codes** | Custom defines | Standard errno |
| **Threading** | Single-threaded | RTOS thread-aware |
| **Debugging** | Direct register access visible | Higher-level API abstraction |
| **Code Size** | Minimal | Includes RTOS overhead |
| **Development** | Low-level learning | Production patterns |

## Cortex-M3 Takeaways

1. **MMIO remains fundamental** even in RTOS environments—peripheral addresses are still physical memory locations
2. **Device Tree abstracts hardware** but doesn't change underlying Cortex-M3 architecture
3. **Read-modify-write is non-atomic** at hardware level—requires careful design in multi-threaded systems
4. **Volatile globals** enable debugging without affecting code semantics
5. **RTOS abstracts timing** but I2C protocol still follows electrical timing requirements
6. **State machines** provide structure for complex embedded operations
7. **Error handling** must account for both software and hardware failures

## References

- [Zephyr I2C API Documentation](https://docs.zephyrproject.org/latest/hardware/peripherals/i2c.html)
- [Zephyr Device Tree Guide](https://docs.zephyrproject.org/latest/build/dts/index.html)
- ARM MPS2/MPS2+ FPGA Prototyping Boards Technical Reference Manual
- ARM Cortex-M3 Technical Reference Manual
- I2C-bus specification and user manual (NXP UM10204)
