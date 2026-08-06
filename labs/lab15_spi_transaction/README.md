# Lab 15: SPI Transaction - Software Bit-Banging Protocol Implementation

## Overview

This lab demonstrates **software-based SPI (Serial Peripheral Interface) protocol implementation** using bit-banging techniques on the ARM Cortex-M3 platform. Unlike hardware SPI peripherals, this lab implements the complete SPI protocol in software, providing full control over timing, clock polarity/phase, and bit ordering.

The lab features a clean driver architecture with hardware abstraction through function pointers, a simulated SPI device that responds to commands, and comprehensive debugging support for understanding the SPI protocol at the bit level.

## Learning Objectives

- Implement SPI protocol entirely in software (bit-banging)
- Master SPI modes (CPOL/CPHA) and their timing differences
- Understand hardware abstraction using function pointers and callbacks
- Design testable embedded drivers with simulated device support
- Control bit ordering (MSB-first vs LSB-first)
- Manage SPI chip select and clock signals manually
- Debug protocol implementations with state machines and volatile variables
- Measure protocol metrics (clock edges, timing delays)
- Test communication protocols without physical hardware

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - Test sequence with state tracking     │
│  - Send READ_ID command (0x9F)          │
│  - Verify device response               │
└──────────────┬──────────────────────────┘
               │ spi_bitbang_transfer()
               ▼
┌─────────────────────────────────────────┐
│  SPI Driver Layer (spi_bitbang.c)       │
│  - Protocol implementation              │
│  - Mode control (CPOL/CPHA)             │
│  - Bit ordering (MSB/LSB first)         │
│  - Chip select management               │
└──────────────┬──────────────────────────┘
               │ ops->set_sclk()
               │ ops->set_mosi()
               │ ops->get_miso()
               │ ops->set_cs()
               ▼
┌─────────────────────────────────────────┐
│  Hardware Abstraction (ops callbacks)    │
│  - Function pointers for portability    │
│  - Context-based operations             │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Simulated Device                        │
│  (simulated_spi_device.c)               │
│  - Responds to READ_ID command          │
│  - Tracks clock edges and delays        │
│  - Returns device ID: 0xEF4018          │
└─────────────────────────────────────────┘
```
## SPI Protocol Fundamentals

### SPI Signal Lines

SPI uses four signal lines for communication:

```
Master                           Slave
┌──────────┐                   ┌──────────┐
│          │ SCLK (Clock)  ->  │          │
│          │                   │          │
│          │ MOSI (Master Out) │          │
│          │      Slave In ->  │          │
│          │                   │          │
│          │ MISO (Master In)  │          │
│          │  <- Slave Out     │          │
│          │                   │          │
│          │ CS (Chip Select)  │          │
│          │              ->   │          │
└──────────┘                   └──────────┘
```

**Signal Definitions**:
- **SCLK**: Serial Clock - Master generates clock, slave synchronizes to it
- **MOSI**: Master Out, Slave In - Data from master to slave
- **MISO**: Master In, Slave Out - Data from slave to master
- **CS**: Chip Select - Activates the slave device (active low typical)

### SPI Modes (CPOL and CPHA)

SPI has four modes based on two parameters:
- **CPOL (Clock Polarity)**: Clock idle state (0 = low, 1 = high)
- **CPHA (Clock Phase)**: Data sampling edge (0 = first edge, 1 = second edge)

```
Mode 0 (CPOL=0, CPHA=0):
SCLK:  ‾‾‾\___/‾‾‾\___/‾‾‾\___/‾‾‾\___
MOSI:  ___X=======X=======X=======X___
         Sample↑   ↑       ↑       ↑
Data changes on falling, samples on rising

Mode 1 (CPOL=0, CPHA=1):
SCLK:  ‾‾‾\___/‾‾‾\___/‾‾‾\___/‾‾‾\___
MOSI:  =======X=======X=======X=======
       ↑Sample   ↑       ↑       ↑
Data changes on rising, samples on falling

Mode 2 (CPOL=1, CPHA=0):
SCLK:  ___/‾‾‾\___/‾‾‾\___/‾‾‾\___/‾‾‾
MOSI:  ___X=======X=======X=======X___
         Sample↑   ↑       ↑       ↑
Data changes on rising, samples on falling

Mode 3 (CPOL=1, CPHA=1):
SCLK:  ___/‾‾‾\___/‾‾‾\___/‾‾‾\___/‾‾‾
MOSI:  =======X=======X=======X=======
       ↑Sample   ↑       ↑       ↑
Data changes on falling, samples on rising
```

**Key Principle**: Data must be stable when it's sampled on the sampling edge.

### SPI Transaction Timing

A complete SPI byte transfer:

```
CS:    ‾‾‾\___________________________/‾‾‾
           ↓ (Select slave)          ↑ (Deselect)

SCLK:  ‾‾‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾‾‾
           0   1   2   3   4   5   6   7

MOSI:  ═══X===X===X===X===X===X===X===X═══
        Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0

MISO:  ═══X===X===X===X===X===X===X===X═══
        Bit7 Bit6 Bit5 Bit4 Bit3 Bit2 Bit1 Bit0
```

**Transfer Sequence**:
1. Assert CS low (select slave)
2. For each bit (typically MSB first):
   - Master sets MOSI to bit value
   - Master toggles SCLK
   - Both devices sample MISO/MOSI on appropriate edge
3. Deassert CS high (deselect slave)

## Cortex-M3 Concepts Covered

### 1. Hardware Abstraction with Function Pointers

The SPI driver uses function pointers to abstract hardware operations:

```c
struct spi_bitbang_ops {
    void (*set_sclk)(void *context, uint8_t level);
    void (*set_mosi)(void *context, uint8_t level);
    void (*set_cs)(void *context, uint8_t level);
    uint8_t (*get_miso)(void *context);
    void (*delay_half_cycle)(void *context);
};

struct spi_bitbang_bus {
    const struct spi_bitbang_ops *ops;
    void *context;
    
    enum spi_mode mode;
    enum spi_bit_order bit_order;
    uint8_t cs_active_low;
};
```

**Benefits**:
- **Portability**: Same driver works with different hardware implementations
- **Testability**: Easy to inject mock/simulated hardware
- **Flexibility**: Runtime selection of hardware backend
- **Separation of concerns**: Protocol logic independent of hardware access

**Usage Pattern**:
```c
// Configure bus with operations
g_spi_bus.ops = simulated_spi_device_get_ops();
g_spi_bus.context = &g_simulated_device;

// Driver calls through function pointers
bus->ops->set_sclk(bus->context, 1);  // Set clock high
uint8_t bit = bus->ops->get_miso(bus->context);  // Read data
```

**Context Pointer**:
- Allows operations to access device-specific state
- Enables multiple SPI buses with different hardware
- Similar to OOP "this" pointer in C

### 2. SPI Mode Implementation

The driver dynamically handles all four SPI modes:

**Key Differences**:
- **CPHA=0**: Data must be stable BEFORE clock edge
- **CPHA=1**: Data changes WITH clock edge, sampled on return
- **Idle level** determined by CPOL
- **Active level** is opposite of idle (`cpol ^ 1`)

### 3. Bit Ordering Control

Supports both MSB-first and LSB-first transmission:

**MSB-First Example** (value = 0xA5 = 10100101):
```
Bit Index:  0    1    2    3    4    5    6    7
Bit Sent:   1    0    1    0    0    1    0    1
Shift:     >>7  >>6  >>5  >>4  >>3  >>2  >>1  >>0
```

**LSB-First Example** (value = 0xA5 = 10100101):
```
Bit Index:  0    1    2    3    4    5    6    7
Bit Sent:   1    0    1    0    0    1    0    1
Shift:     >>0  >>1  >>2  >>3  >>4  >>5  >>6  >>7


**Why Both Orders Matter**:
- Most SPI devices use MSB-first (default)
- Some legacy or specialized devices require LSB-first
- Software implementation makes both equally easy
- Hardware SPI peripherals often only support one order

### 4. Chip Select Management

Proper CS timing is critical for SPI communication:

**CS Polarity**:
- Most SPI devices use **active-low** CS (low = selected)
- Some devices use **active-high** CS (high = selected)
- Driver supports both through `cs_active_low` flag

**CS Timing Rules**:
1. Assert CS before first clock edge
2. Keep CS asserted during entire transaction
3. Return clock to idle level before deasserting CS
4. Deassert CS after last clock edge completes

**Multi-Slave Selection**:
```c
// Hardware typically has separate CS line per slave
CS0: ‾‾\___________/‾‾‾‾‾‾‾‾‾‾‾‾  (Slave 0 selected)
CS1: ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾  (Slave 1 idle)
CS2: ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾  (Slave 2 idle)
```

### 5. Byte Transfer Implementation

Complete byte transfer with simultaneous TX and RX:


**Full-Duplex Communication**:
- SPI is inherently full-duplex
- Master sends and receives simultaneously
- Every TX byte produces an RX byte (even if garbage)
- Dummy bytes (0xFF) used when only reading

### 6. Transaction-Level Transfer

Multi-byte transfer with optional TX/RX buffers:


**Flexible Buffer Handling**:
- `tx_buffer == NULL`: Send 0xFF (read-only operation)
- `rx_buffer == NULL`: Ignore received data (write-only)
- Both provided: Full-duplex transfer
- CS asserted for entire multi-byte transfer

### 7. Simulated Device for Testing

The lab includes a complete simulated SPI flash device:

**Why Simulation**:
- Test driver without physical hardware
- Validate protocol timing and sequencing
- Debug driver logic independently
- Measure performance metrics
- Educational tool for understanding protocol

### 8. State Machine Debugging

Application uses explicit state tracking for debugging:

```c
enum lab15_stage {
    LAB15_STAGE_RESET = 0,
    LAB15_STAGE_INIT,
    LAB15_STAGE_TRANSFER,
    LAB15_STAGE_VERIFY,
    LAB15_STAGE_DONE,
    LAB15_STAGE_ERROR
};
```

**Why This Pattern**:
- `volatile` prevents compiler optimization
- Checkpoint provides consistent breakpoint location
- State enum gives meaningful context in debugger
- Separate result variables track each operation
- Metrics capture performance characteristics

## Code Walkthrough

### READ_ID Command Transaction

**Command**: 0x9F (JEDEC Read ID)  
**Purpose**: Read flash device manufacturer and device ID

**Transaction Sequence**:
```
Master sends:  [0x9F] [0xFF] [0xFF] [0xFF]
Slave returns: [0xFF] [0xEF] [0x40] [0x18]
                 ↑      ↑      ↑      ↑
              dummy   Mfr ID  Type  Capacity
```

**SPI Bus Timing** (Mode 0, MSB-first):
```
CS:   ‾‾\___________________________________/‾‾

      |<---- Byte 0 ---->|<---- Byte 1 ---->|
SCLK: ‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾\_/‾
       0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7

MOSI: X1=0=0=1=1=1=1=1=XXXXXXXXXXXXXXXXXXXXXXX
       ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
       0x9F command

MISO: XXXXXXXXXXXXXXXXXXXXXXX1=1=1=0=1=1=1=1XXX
                              ‾‾‾‾‾‾‾‾‾‾‾‾‾‾‾
                              0xEF response
```


### GDB Debugging Session

**Set breakpoints and inspect state**:

```gdb
(gdb) break lab15_debug_checkpoint
Breakpoint 1 at 0x320: file main.c, line 24.

(gdb) continue

# At INIT stage
(gdb) print g_lab15_stage
$1 = LAB15_STAGE_INIT

(gdb) continue

# At TRANSFER stage
(gdb) print g_lab15_stage
$2 = LAB15_STAGE_TRANSFER

(gdb) print/x {g_spi_tx[0], g_spi_tx[1], g_spi_tx[2], g_spi_tx[3]}
$3 = {0x9f, 0xff, 0xff, 0xff}

(gdb) continue

# After transfer completes
(gdb) print g_lab15_stage
$4 = LAB15_STAGE_DONE

(gdb) print/x {g_spi_rx[0], g_spi_rx[1], g_spi_rx[2], g_spi_rx[3]}
$5 = {0xff, 0xef, 0x40, 0x18}

(gdb) print g_spi_clock_edges
$6 = 64

(gdb) print g_spi_delay_calls
$7 = 64
```

**Trace protocol execution**:
```gdb
# Break on bit-level operations
(gdb) break spi_bitbang_transfer_byte
(gdb) break simulated_get_miso

# Step through byte transfer
(gdb) continue
(gdb) step
(gdb) print/x tx_byte
(gdb) print/x rx_byte

# Watch MISO changes
(gdb) watch g_simulated_device.miso
```

**Inspect simulated device state**:
```gdb
(gdb) print g_simulated_device
$8 = {
  sclk = 0,
  mosi = 1,
  miso = 1,
  cs = 1,
  clock_edge_count = 64,
  delay_count = 64,
  selected = 0,
  command_received = 1,
  response_byte_index = 3,
  response_bit_index = 0,
  response = {0xef, 0x40, 0x18}
}
```

## Key Observations

### 1. SPI vs I2C Comparison

| Feature | SPI | I2C |
|---------|-----|-----|
| **Signals** | 4 (SCLK, MOSI, MISO, CS) | 2 (SDA, SCL) |
| **Topology** | Point-to-point or bus | Multi-master bus |
| **Addressing** | CS pin per device | 7-bit address |
| **Speed** | Higher (MHz typical) | Lower (100-400 kHz) |
| **Full-Duplex** | Yes | No |
| **Clock** | Master only | Multi-master |
| **ACK** | No | Yes (every byte) |
| **Complexity** | Simpler protocol | More complex |
| **Pin Usage** | 3 + N (N slaves) | 2 (all slaves) |

### 2. Software Bit-Banging Trade-offs

**Advantages**:
- ✅ Full control over timing
- ✅ Support all four SPI modes
- ✅ Support any bit ordering
- ✅ Use any GPIO pins
- ✅ Multiple buses possible
- ✅ No hardware dependency
- ✅ Educational value
- ✅ Custom protocol variations

**Disadvantages**:
- ❌ CPU intensive (100% during transfer)
- ❌ Slower than hardware SPI
- ❌ Affected by interrupts
- ❌ No DMA support
- ❌ Larger code size
- ❌ Higher power consumption
- ❌ Timing less precise

### 3. Clock Edge Calculation

For 4-byte transfer (READ_ID command):
```
Bytes: 4
Bits per byte: 8
Total bits: 4 × 8 = 32

Clock transitions per bit: 2 (low→high, high→low)
Total clock edges: 32 × 2 = 64

Delay calls per bit: 2 (before each edge)
Total delay calls: 32 × 2 = 64
```

### 4. Timing Analysis

**Per-Bit Timing** (Mode 0, CPHA=0):
```
1. Set MOSI to bit value
2. delay_half_cycle()         ← 1st delay
3. Set SCLK active (high)
4. Read MISO
5. delay_half_cycle()         ← 2nd delay
6. Set SCLK idle (low)
```

**Timing with Cortex-M3 at 25 MHz**:
```
NOP instruction: 1 cycle = 40 ns
delay_half_cycle with 10 NOPs: ~400 ns
Bit time: 2 × 400 ns = 800 ns
Bit rate: 1.25 Mbps
Byte rate: 156.25 kB/s
```

**With function call overhead** (~20-50 cycles):
```
Realistic bit time: ~2-3 μs
Effective SPI clock: ~300-500 kHz
```

## Key Takeaways

### 1. Hardware Abstraction Patterns
- **Function pointers** enable portability and testability
- **Context pointers** allow multiple device instances
- **Operations structure** separates interface from implementation
- Similar to OOP virtual methods in C

### 2. Protocol State Machines
- SPI requires precise bit-level state management
- Mode parameters (CPOL/CPHA) fundamentally change timing
- Clock and data must be synchronized carefully
- CS timing is critical for transaction boundaries

### 3. Bit-Level Manipulation
- Efficient bit extraction using shifts and masks
- Support for MSB-first and LSB-first ordering
- Byte assembly from individual bits
- Understanding binary representation essential

### 4. Software Timing Control
- `volatile` prevents compiler optimization of delays
- Inline assembly provides cycle-accurate timing
- Function call overhead matters at high speeds
- Interrupts can disrupt timing-critical code

### 5. Testing Without Hardware
- Simulated devices validate driver logic
- Metrics (clock edges, delays) verify correct operation
- State tracking enables protocol debugging
- Same driver works with simulation and real hardware

### 6. Debugging Embedded Protocols
- State machines provide visibility into execution flow
- `volatile` globals expose internal state to GDB
- Checkpoint functions create consistent breakpoint targets
- `noinline` attribute ensures functions exist for debugging

### 7. Full-Duplex Communication
- SPI transmits and receives simultaneously
- Every transmitted byte produces a received byte
- Dummy bytes (0xFF) used for read-only operations
- Master controls all timing

### 8. Real-World Protocol Implementation
- Flash memory uses standard command set
- Device identification via READ_ID command
- Multi-byte transfers for complex operations
- CS must remain asserted during transaction

## When to Use Software SPI vs Hardware SPI

### Choose Software SPI When:

✅ **Hardware limitations**
- All hardware SPI peripherals already in use
- Need more SPI buses than hardware provides
- GPIO pins available but no SPI peripheral

✅ **Flexibility requirements**
- Need to support non-standard SPI modes
- Custom timing requirements
- Non-standard bit ordering

✅ **Development/Testing**
- Prototyping new SPI devices
- Testing driver logic without hardware
- Learning SPI protocol details
- Debugging protocol issues

✅ **Bus recovery**
- Need manual control for stuck devices
- Custom error handling required

### Choose Hardware SPI When:

✅ **Performance critical**
- High-speed transfers (>1 MHz)
- Large data volumes
- Real-time constraints

✅ **Power efficiency**
- Battery-powered devices
- CPU needs to sleep during transfers
- System power budget tight

✅ **CPU bandwidth limited**
- Many simultaneous tasks
- CPU-intensive application
- Real-time OS with scheduling

✅ **DMA support needed**
- Zero-CPU transfers
- Background data movement
- Interrupt-driven architecture

✅ **Production systems**
- Proven hardware reliability
- Industry-standard implementation
- Reduced code complexity

## Performance Comparison

### Software SPI (This Lab)
```
Clock Speed:     ~300-500 kHz (typical)
CPU Usage:       100% during transfer
Transfer 1 KB:   ~20-30 ms
Code Size:       ~2 KB
Power:           High (CPU active)
Flexibility:     Maximum
```

### Hardware SPI (Typical Cortex-M3)
```
Clock Speed:     Up to 18 MHz (72 MHz / 4)
CPU Usage:       5-10% (interrupt) or 0% (DMA)
Transfer 1 KB:   ~0.5 ms
Code Size:       ~500 bytes (driver)
Power:           Low (CPU can sleep)
Flexibility:     Limited to hardware modes
```

### Use Case Comparison

**Software SPI Good For**:
- Sensor initialization (low data volume)
- Display controllers (moderate speed OK)
- SD card initialization (slow mode)
- Educational purposes
- Custom protocols

**Hardware SPI Good For**:
- SD card data transfers (high speed)
- Audio codecs (continuous streaming)
- High-speed ADCs/DACs
- Network interfaces (Ethernet, WiFi)
- Display frame buffers

## References

### SPI Protocol
- [SPI Wikipedia](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface)
- [SPI Protocol Guide by Analog Devices](https://www.analog.com/en/analog-dialogue/articles/introduction-to-spi-interface.html)
- [Motorola SPI Block Guide](https://www.nxp.com/docs/en/data-sheet/S08SH4.pdf)

### ARM Cortex-M3
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM Thumb-2 Instruction Set](https://developer.arm.com/documentation/ddi0308/latest/)

### SPI Flash Devices
- [Winbond W25Q Series Datasheet](https://www.winbond.com/resource-files/w25q32jv%20revg%2003272018%20plus.pdf)
- [JEDEC JESD216 (Serial Flash Discoverable Parameters)](https://www.jedec.org/standards-documents/docs/jesd216b)

### Embedded Software Patterns
- [Embedded Software Design Patterns](https://www.embedded.com/design-patterns-for-embedded-systems-in-c/)
- [Making Embedded Systems by Elecia White](https://www.oreilly.com/library/view/making-embedded-systems/9781449308889/)

