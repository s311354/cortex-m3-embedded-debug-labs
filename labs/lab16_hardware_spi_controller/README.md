# Lab 16: Hardware SPI Controller - ARM PrimeCell SSP (PL022)

## Overview

This lab demonstrates **hardware-based SPI communication** using the ARM PrimeCell Synchronous Serial Port (PL022 SSP) controller on the MPS2+ FPGA platform. Unlike Lab 15's software bit-banging approach, this lab uses a dedicated hardware peripheral with integrated FIFOs, clock generation, and protocol state machines.

## Learning Objectives

- Program ARM PrimeCell SSP (PL022) hardware SPI controller
- Master MMIO register-level peripheral configuration
- Understand hardware FIFO-based data transfer
- Configure SPI clock generation with prescalers
- Implement SPI mode control (CPOL/CPHA) via hardware registers
- Use hardware loopback mode for driver testing
- Design layered driver architecture (HAL, Board, Application)
- Configure interrupt sources through peripheral registers
- Debug hardware peripheral state with register snapshots
- Understand polling vs interrupt-driven I/O patterns
- Compare hardware vs software SPI implementations

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - Test sequence with stages            │
│  - Loopback data verification           │
│  - Register state capture                │
│  - Debug checkpoints                     │
└──────────────┬──────────────────────────┘
               │ mps2_ssp_init()
               │ mps2_ssp_transfer()
               │ mps2_ssp_enable_interrupts()
               ▼
┌─────────────────────────────────────────┐
│  HAL Driver Layer (mps2_ssp.c)          │
│  - Hardware initialization              │
│  - Clock configuration                  │
│  - FIFO polling and transfer            │
│  - Interrupt mask control               │
│  - Timeout handling                     │
└──────────────┬──────────────────────────┘
               │ Register access
               ▼
┌─────────────────────────────────────────┐
│  Board Support Layer (board_ssp.c)      │
│  - Platform-specific config             │
│  - SSP3 instance setup                  │
│  - Clock source configuration           │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  PL022 SSP Hardware Registers           │
│  - CR0: Control Register 0              │
│  - CR1: Control Register 1              │
│  - DR: Data Register (FIFO)             │
│  - SR: Status Register                  │
│  - CPSR: Clock Prescale Register        │
│  - IMSC: Interrupt Mask                 │
│  - RIS/MIS: Interrupt Status            │
│  - DMACR: DMA Control                   │
└─────────────────────────────────────────┘
```


## PL022 SSP Hardware Overview

### Key Features

The ARM PrimeCell Synchronous Serial Port (PL022) provides:

- **Motorola SPI protocol support** (Frame Format)
- **Master or Slave operation** (configured as Master)
- **4-16 bit data size** (configured for 8-bit)
- **Programmable bit rate** via dual prescaler system
- **Integrated TX/RX FIFOs** (8 entries each)
- **Four SPI modes** (Mode 0-3 via CPOL/CPHA)
- **Hardware loopback mode** for testing
- **Interrupt support** (TX, RX, timeout, overrun)

### Register Map

| Offset | Register | Name | Description |
|--------|----------|------|-------------|
| 0x000 | CR0 | Control Register 0 | Data size, frame format, clock parameters |
| 0x004 | CR1 | Control Register 1 | SSP enable, master/slave, loopback |
| 0x008 | DR | Data Register | TX/RX FIFO access (write TX, read RX) |
| 0x00C | SR | Status Register | FIFO status, busy flag |
| 0x010 | CPSR | Clock Prescale | Even divider 2-254 |
| 0x014 | IMSC | Interrupt Mask | Enable/disable interrupt sources |
| 0x018 | RIS | Raw Interrupt Status | Raw interrupt flags |
| 0x01C | MIS | Masked Interrupt Status | After masking |
| 0x020 | ICR | Interrupt Clear | Clear interrupt flags |
| 0x024 | DMACR | DMA Control | DMA TX/RX enable |

### Clock Generation

The PL022 uses a two-stage prescaler for flexible clock generation:

```
SSP_CLK = INPUT_CLOCK / (CPSDVSR × (SCR + 1))

Where:
  INPUT_CLOCK = PCLK (typically 25 MHz on MPS2)
  CPSDVSR = Clock prescale divisor (even 2-254) [CPSR register]
  SCR = Serial clock rate (0-255) [CR0[15:8]]
```

**Example** (this lab's configuration):
```
INPUT_CLOCK = 25 MHz
CPSDVSR = 8
SCR = 3

SSP_CLK = 25,000,000 / (8 × (3 + 1))
        = 25,000,000 / 32
        = 781,250 Hz
        = 781.25 kHz
```

**Clock Rate Selection Strategy**:
1. Choose CPSDVSR (coarse adjustment): Even numbers 2-254
2. Choose SCR (fine adjustment): 0-255
3. Lower values = faster clock (up to INPUT_CLOCK / 2)
4. Both prescalers multiply for flexibility


### SPI Mode Configuration (CR0 Register)

**Control Register 0 (CR0)** configures the SPI protocol parameters:

```
CR0 [31:0]:
  [15:8] SCR    - Serial Clock Rate (0-255)
  [7]    SPH    - SSPCLKOUT phase (CPHA)
  [6]    SPO    - SSPCLKOUT polarity (CPOL)
  [5:4]  FRF    - Frame format (00=SPI, 01=TI SSI, 10=Microwire)
  [3:0]  DSS    - Data size select (0011=4-bit ... 1111=16-bit)
```

**SPI Mode Encoding**:

| Mode | CPOL (SPO) | CPHA (SPH) | CR0 Bits | Clock Idle | Sample Edge |
|------|------------|------------|----------|------------|-------------|
| 0 | 0 | 0 | Neither set | Low | Rising |
| 1 | 0 | 1 | SPH set | Low | Falling |
| 2 | 1 | 0 | SPO set | High | Falling |
| 3 | 1 | 1 | Both set | High | Rising |


### Control Register 1 (CR1)

**CR1** controls SSP operation mode and enable:

```
CR1 [31:0]:
  [3]  SOD  - Slave output disable
  [2]  MS   - Master/Slave select (0=Master, 1=Slave)
  [1]  SSE  - SSP Enable (1=Enabled)
  [0]  LBM  - Loopback mode (1=Enabled, MISO connected to MOSI internally)
```

### Status Register (SR)

**SR** provides real-time FIFO and bus status:

```
SR [31:0]:
  [4] BSY  - SSP busy flag (1=transmitting/receiving)
  [3] RFF  - Receive FIFO full
  [2] RNE  - Receive FIFO not empty (data available)
  [1] TNF  - Transmit FIFO not full (can write)
  [0] TFE  - Transmit FIFO empty
```

## FIFO Architecture

### TX/RX FIFO Overview

The PL022 contains separate 8-entry FIFOs for transmit and receive:

```
Application Layer
       ↓ Write              ↑ Read
   ┌─────────┐          ┌─────────┐
   │ TX FIFO │          │ RX FIFO │
   │ 8 × 16b │          │ 8 × 16b │
   └────┬────┘          └────▲────┘
        ↓ Hardware          │ Hardware
    ┌────────────────────────┐
    │   SPI Shift Register   │
    └────────────────────────┘
            ↓         ↑
          MOSI      MISO
```

**FIFO Benefits**:
- **Reduced CPU overhead**: Batch multiple bytes
- **Improved throughput**: Continuous transmission
- **Interrupt efficiency**: Less frequent interrupts
- **Burst transfers**: Write/read multiple bytes at once
- **Hardware flow control**: Automatic pacing

**FIFO Depths** (PL022):
- TX FIFO: 8 entries × 16 bits
- RX FIFO: 8 entries × 16 bits
- Configurable data width (4-16 bits)


## Cortex-M3 Concepts Covered

### 1. Hardware Peripheral Register Access

Direct MMIO access through typed structure pointers:

```c
typedef struct {
    __IO uint32_t CR0;      // Control Register 0
    __IO uint32_t CR1;      // Control Register 1
    __IO uint32_t DR;       // Data Register
    __I  uint32_t SR;       // Status Register (read-only)
    __IO uint32_t CPSR;     // Clock Prescale
    __IO uint32_t IMSC;     // Interrupt Mask
    __I  uint32_t RIS;      // Raw Interrupt Status
    __I  uint32_t MIS;      // Masked Interrupt Status
    __O  uint32_t ICR;      // Interrupt Clear
    __IO uint32_t DMACR;    // DMA Control
} MPS2_SSP_TypeDef;

// Memory-mapped instance
#define MPS2_SSP3 ((MPS2_SSP_TypeDef *)0x40023000)
```

**CMSIS IO Qualifiers**:
- `__IO` - Read/Write register
- `__I` - Read-only register
- `__O` - Write-only register
- Volatile semantics enforced

**Why This Pattern**:
- Type-safe register access
- Compiler enforces read/write permissions
- Standard CMSIS pattern across all ARM devices
- Hardware abstraction without overhead
- Single memory address → peripheral base

### 2. Bit Field Manipulation

Standard embedded pattern for register configuration:

```c
// Using mask and shift macros
#define SSP_CR0_DSS_Pos  0
#define SSP_CR0_DSS_Msk  (0xFU << SSP_CR0_DSS_Pos)
#define SSP_CR0_SCR_Pos  8
#define SSP_CR0_SCR_Msk  (0xFFU << SSP_CR0_SCR_Pos)

// Building register value
uint32_t cr0 = 0;
cr0 |= ((data_bits - 1U) << SSP_CR0_DSS_Pos);  // Data size
cr0 |= (scr << SSP_CR0_SCR_Pos);                // Clock rate
cr0 |= SSP_CR0_SPH_Msk;                         // Set CPHA bit

```

### 3. Polling vs Interrupt-Driven I/O

**Polling Pattern** (this lab):

**Advantages**:
- ✅ Simple logic flow
- ✅ Easy to debug
- ✅ No interrupt configuration needed
- ✅ Deterministic timing for small transfers

**Disadvantages**:
- ❌ CPU busy-waits (100% usage)
- ❌ Cannot do other work during transfer
- ❌ Power inefficient
- ❌ Blocks other tasks

**Interrupt Pattern** (prepared in driver):

**Advantages**:
- ✅ CPU free for other tasks
- ✅ Power efficient (sleep between interrupts)
- ✅ Better for continuous streaming
- ✅ Scalable to multiple peripherals

**Disadvantages**:
- ❌ More complex code
- ❌ Race conditions possible
- ❌ Interrupt latency affects timing
- ❌ Harder to debug


### 4. Three-Layer Driver Architecture

**Layer Separation**:

```
┌────────────────────────────────────┐
│ Application (main.c)               │  ← High-level test logic
│ - Business logic                   │
│ - Test sequences                   │
│ - Result validation                │
└─────────────┬──────────────────────┘
              │ HAL API calls
┌─────────────▼──────────────────────┐
│ HAL Driver (mps2_ssp.c)            │  ← Hardware abstraction
│ - mps2_ssp_init()                  │
│ - mps2_ssp_transfer()              │
│ - mps2_ssp_enable_interrupts()     │
└─────────────┬──────────────────────┘
              │ Uses board config
┌─────────────▼──────────────────────┐
│ Board Support (board_ssp.c)        │  ← Platform config
│ - g_board_ssp3 instance            │
│ - g_board_ssp3_config              │
│ - Clock settings                   │
└────────────────────────────────────┘
```

**Benefits**:
- **Portability**: Change board without changing HAL
- **Reusability**: Same HAL for different platforms
- **Testability**: Mock board layer for unit tests
- **Maintainability**: Clear separation of concerns

### 5. Hardware Loopback Testing

Internal loopback mode connects MOSI to MISO inside the peripheral:

```
Without Loopback:
┌─────────┐  MOSI  ┌─────────┐
│ Master  │───────>│  Slave  │
│   TX    │        │   RX    │
│   RX    │<───────│   TX    │
└─────────┘  MISO  └─────────┘

With Loopback (LBM=1):
┌───────────────────┐
│    SSP Master     │
│  TX ──┐           │
│       │ Internal  │
│  RX <─┘ loopback  │
└───────────────────┘
```

**Advantages**:
- ✅ Test driver without external hardware
- ✅ Verify TX and RX paths
- ✅ Check clock generation
- ✅ Validate FIFO operation
- ✅ Development without prototype hardware


### 6. Debug Infrastructure

Multiple debugging aids for hardware bring-up:

**Debug Checkpoints**:
- Prevents inlining (guaranteed address for breakpoint)
- Consistent location across compilations
- NOP prevents side effects

**Why This Pattern**:
- `volatile` prevents optimization
- Visible in GDB without symbols
- Captures state at specific points
- Easy post-mortem debugging
- Common embedded debugging technique


### Basic Debugging

```gdb
# Set breakpoints
(gdb) break debug_checkpoint
Breakpoint 1 at 0x4b8: file main.c, line 25.

(gdb) break main
Breakpoint 2 at 0x4bc: file main.c, line 31.

# Start execution
(gdb) continue
Breakpoint 2, main () at main.c:31

# Examine initial state
(gdb) print g_stage
$1 = 0

(gdb) print/x {tx_buffer[0], tx_buffer[1], tx_buffer[2], tx_buffer[3]}
$2 = {0x0, 0x0, 0x0, 0x0}
```

### Trace Through Stages

```gdb
# Continue to first checkpoint (after init)
(gdb) continue
Breakpoint 1, debug_checkpoint () at main.c:25

(gdb) print g_stage
$3 = 1

(gdb) print init_result
$4 = 0  # MPS2_SSP_OK

# Examine hardware registers after init
(gdb) print/x reg_cr0
$5 = 0x307  # SCR=3, FRF=0, SPO=0, SPH=0, DSS=7

(gdb) print/x reg_cr1
$6 = 0x3  # SSE=1, LBM=1, MS=0

(gdb) print/x reg_cpsr
$7 = 0x8  # CPSDVSR=8

(gdb) print g_board_ssp3.actual_clock_hz
$8 = 781250  # 25MHz / (8 × 4) = 781.25 kHz

# Continue to second checkpoint (after transfer)
(gdb) continue
Breakpoint 1, debug_checkpoint () at main.c:25

(gdb) print g_stage
$9 = 2

(gdb) print transfer_result
$10 = 0  # MPS2_SSP_OK

# Verify loopback worked
(gdb) print/x {tx_buffer[0], tx_buffer[1], tx_buffer[2], tx_buffer[3]}
$11 = {0x9f, 0xa5, 0x5a, 0xff}

(gdb) print/x {rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]}
$12 = {0x9f, 0xa5, 0x5a, 0xff}  # Matches TX!

(gdb) print verify_result
$13 = 0  # Success
```

### Examine Registers During Transfer

```gdb
# Set breakpoint in transfer function
(gdb) break mps2_ssp_transfer
Breakpoint 3 at 0x3a0: file mps2_ssp.c, line 69.

(gdb) continue
Breakpoint 3, mps2_ssp_transfer () at mps2_ssp.c:69

# Step into first byte transfer
(gdb) next
(gdb) next

# Examine DR register (data register)
(gdb) print/x ssp->regs->DR
$14 = 0x9f  # First TX byte written

# Check status register
(gdb) print/x ssp->regs->SR
$15 = 0x3  # TNF=1, TFE=1 (TX FIFO not full, empty)

# Continue until RX data available
(gdb) next
(gdb) next

(gdb) print/x ssp->regs->SR
$16 = 0x4  # RNE=1 (RX FIFO not empty)

(gdb) print/x ssp->regs->DR
$17 = 0x9f  # Loopback data matches!
```

### Interrupt Configuration Test

```gdb
# Continue to interrupt test stage
(gdb) continue
Breakpoint 1, debug_checkpoint () at main.c:25

(gdb) print g_stage
$18 = 3

# Check interrupt mask before enable
(gdb) print/x reg_imsc
$19 = 0x0  # All interrupts disabled

(gdb) continue
Breakpoint 1, debug_checkpoint () at main.c:25

# Check after enable
(gdb) print/x g_board_ssp3.regs->IMSC
$20 = 0x6  # RXIM=1, RTIM=1 (bits 2 and 1)

(gdb) continue
Breakpoint 1, debug_checkpoint () at main.c:25

(gdb) print g_stage
$21 = 4

# Check after disable
(gdb) print/x g_board_ssp3.regs->IMSC
$22 = 0x0  # Interrupts disabled again
```

### Disassemble Key Functions

```gdb
# Examine SSP register access
(gdb) disassemble mps2_ssp_init
   0x00000350 <+0>:     push    {r4, r5, lr}
   0x00000352 <+2>:     ldr     r3, [r0, #0]      # Load regs pointer
   0x00000354 <+4>:     ldr     r2, [r3, #4]      # Load CR1
   0x00000356 <+6>:     bic     r2, r2, #2        # Clear SSE bit
   0x00000358 <+8>:     str     r2, [r3, #4]      # Write CR1
   ...

# Look at FIFO write
(gdb) disassemble /r mps2_ssp_transfer
   0x000003a8 <+24>:    str     r2, [r3, #8]     # Write to DR (FIFO)
   0x000003aa <+26>:    ldr     r3, [r0, #0]     # Load regs
   0x000003ac <+28>:    ldr     r2, [r3, #12]    # Read SR
   ...
```

## Hardware vs Software SPI Comparison

### Lab 15 (Software Bit-Banging) vs Lab 16 (Hardware Controller)

| Aspect | Software (Lab 15) | Hardware (Lab 16) |
|--------|-------------------|-------------------|
| **Implementation** | GPIO bit manipulation | PL022 peripheral registers |
| **Clock Speed** | ~300-500 kHz | Up to 12.5 MHz (25MHz/2) |
| **CPU Usage** | 100% during transfer | <5% (polling), 0% (DMA) |
| **Code Size** | ~2 KB | ~1 KB (driver) |
| **FIFO** | None | 8×16-bit TX/RX |
| **Interrupts** | Manual | Hardware support |
| **DMA** | Not possible | Supported |
| **Timing Precision** | Function call overhead | Hardware state machine |
| **Power** | High (CPU active) | Low (CPU can sleep) |
| **Flexibility** | Full control | Limited to hardware modes |
| **Pins Used** | Any GPIO | Dedicated SSP pins |
| **Multiple Buses** | Limited by GPIO | Multiple SSP peripherals |

### When to Use Hardware SPI

**✅ Choose Hardware SPI When**:
- High-speed transfers required (>1 MHz)
- Large data volumes (display frames, audio)
- Power efficiency critical
- CPU bandwidth limited
- DMA transfers needed
- Standard SPI modes sufficient
- Production systems

**❌ Avoid Hardware When**:
- All peripherals already allocated
- Need more SPI buses than available
- Non-standard timing required
- Custom protocol variations
- Development/prototyping only

### Performance Analysis

**Throughput Comparison**:

```
Software SPI (Lab 15):
  Clock: ~400 kHz
  Byte time: ~20 μs
  1 KB transfer: ~20 ms
  CPU cycles (25 MHz): 500,000 cycles

Hardware SPI (Lab 16):
  Clock: 781.25 kHz (configured)
  Byte time: ~10 μs
  1 KB transfer: ~10 ms
  CPU cycles (polling): ~50,000 cycles
  Speedup: ~10× CPU efficiency
```

## Key Observations

### 1. Register-Level Hardware Control

The PL022 SSP requires precise register configuration sequence:
1. **Disable** peripheral (CR1.SSE = 0)
2. **Configure** all registers
3. **Enable** peripheral (CR1.SSE = 1)

This pattern is common in ARM peripherals to prevent glitches during reconfiguration.

### 2. Clock Generation Flexibility

The dual prescaler system (CPSR × CR0.SCR) provides fine clock control:
- 510 possible divider combinations
- Clock range: PCLK/2 to PCLK/65,024
- Suitable for various SPI device speeds

### 3. FIFO-Based Architecture

Hardware FIFOs significantly reduce interrupt frequency:
- **Without FIFO**: Interrupt per byte (1 KB = 1024 interrupts)
- **With FIFO**: Interrupt per 8 bytes (1 KB = 128 interrupts)
- **Reduction**: 87.5% fewer interrupts

### 4. Loopback as Development Tool

Internal loopback mode is invaluable for:
- Driver development without hardware
- Automated testing in CI/CD
- Hardware bring-up verification
- Teaching SPI concepts

### 5. Interrupt vs Polling Trade-offs

**Polling** (this lab):
- Simple, synchronous
- Good for small transfers (<256 bytes)
- No interrupt configuration complexity

**Interrupt** (prepared for):
- Asynchronous, efficient
- Good for large/continuous transfers
- Requires careful state management

## Key Takeaways

### 1. Peripheral Programming Patterns
- Disable-configure-enable sequence prevents glitches
- Single register writes (build value, then write)
- Timeout protection on all polling loops
- Status flags before data access

### 2. Hardware Abstraction
- Three-layer architecture (App, HAL, Board)
- Configuration structures separate from instances
- Platform-specific constants isolated in board layer
- CMSIS standard register definitions

### 3. FIFO Management
- Check status before access (TNF, RNE)
- Hardware handles pacing automatically
- Reduces interrupt frequency dramatically
- Enables burst transfers

### 4. Debugging Techniques
- `volatile` globals for GDB visibility
- `noinline` functions for consistent breakpoints
- Register snapshots at key points
- Stage tracking for execution flow
- Result variables for error diagnosis

### 5. Production Considerations
- Always implement timeouts
- Return specific error codes
- Document hardware assumptions
- Support runtime configuration
- Provide self-test mechanisms (loopback)

### 6. ARM Cortex-M3 Features Used
- Memory-mapped peripheral access
- CMSIS register definitions
- Atomic register operations
- Efficient bit manipulation
- Interrupt controller integration

## References

### ARM PrimeCell SSP (PL022)
- [PL022 Technical Reference Manual](https://developer.arm.com/documentation/ddi0194/latest/)
- [ARM Primecell Synchronous Serial Port (PL022)](https://developer.arm.com/ip-products/peripherals/primecell-peripherals/pl022)

### ARM Cortex-M3
- [Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [Cortex-M3 Devices Generic User Guide](https://developer.arm.com/documentation/dui0552/latest/)

### CMSIS
- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/)
- [CMSIS Device Template](https://arm-software.github.io/CMSIS_5/Core/html/device_h_pg.html)

### MPS2+ Platform
- [ARM MPS2+ FPGA Prototyping Board](https://developer.arm.com/tools-and-software/development-boards/fpga-prototyping-boards/mps2)
- [AN385 - ARM Cortex-M3 SMM on V2M-MPS2](https://developer.arm.com/documentation/dai0385/latest/)

### SPI Protocol
- [SPI Protocol Wikipedia](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface)
- [Motorola SPI Block Guide](https://www.nxp.com/docs/en/data-sheet/MC68HC11E.pdf)

### Embedded Systems
- [Making Embedded Systems by Elecia White](https://www.oreilly.com/library/view/making-embedded-systems/9781449308889/)
- [Embedded Software Primer by David E. Simon](https://www.pearson.com/en-us/subject-catalog/p/embedded-software-primer/P200000003312)
