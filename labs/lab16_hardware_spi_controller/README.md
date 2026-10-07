# Lab 16: Hardware SPI Controller - ARM PrimeCell SSP (PL022)

## Overview

This lab demonstrates **MMIO-based programming of the ARM PrimeCell Synchronous Serial Port (PL022 SSP)** using the MPS2/AN385 memory map. The supplied Makefile runs the Cortex-M3 firmware under QEMU's `mps2-an385` machine. The default test selects **SSP3 / Shield0 at `0x40026000`, Mode 0, 8-bit data, internal loopback, and polling**; it does not communicate with an external SPI target.

Unlike Lab 15's in-firmware scripted responder, Lab 16 accesses a peripheral register interface. On hardware, PL022 supplies serial shifting, FIFOs, and clock generation. QEMU's PL022 model implements the register interface, FIFOs, and loopback behavior, but it does not emulate serial line speed and ignores the programmed clock rate and frame format. A passing QEMU loopback test is therefore not a measurement of physical SCLK timing, CPOL/CPHA behavior, pin routing, power, or CPU utilization.

## Learning Objectives

- Program ARM PrimeCell SSP (PL022) hardware SPI controller
- Master MMIO register-level peripheral configuration
- Understand hardware FIFO-based data transfer
- Configure SPI clock generation with prescalers
- Implement SPI mode control (CPOL/CPHA) via hardware registers
- Use the controller's internal loopback mode for driver testing
- Design layered driver architecture (HAL, Board, Application)
- Exercise interrupt-mask registers without implementing an interrupt-driven transfer
- Debug hardware peripheral state with register snapshots
- Distinguish the implemented polling path from a possible interrupt-driven extension
- Compare hardware vs software SPI implementations

## Architecture Overview

```
┌───────────────────────────────────────────┐
│  Application Layer (main.c)               │
│  - Test sequence, comparison, snapshots   │
│  - Calls board_ssp3_init() to initialize  │
│  - Calls HAL directly for transfer/masking│
└──────────────┬────────────────────────────┘
               │ initialization
               ▼
┌───────────────────────────────────────────┐
│  Board Support Layer (board_ssp.c)        │
│  - Selects SSP3 and default configuration │
│  - Copies SystemCoreClock into config     │
│  - Calls mps2_ssp_init()                  │
└──────────────┬────────────────────────────┘
               │ config + instance
               ▼
┌───────────────────────────────────────────────┐
│  HAL Driver Layer (mps2_ssp.c)                │
│  - Register configuration                     │
│  - One-byte-at-a-time FIFO polling            │
│  - Poll-iteration timeout counters            │
│  - Interrupt-mask read-modify-write helpers   │
└──────────────┬────────────────────────────────┘
               │ MMIO reads/writes
               ▼
┌─────────────────────────────────────────────┐
│  SSP3 / Shield0 register block: 0x40026000  │
│  - CR0 / CR1: configuration                 │
│  - DR: write TX FIFO / read RX FIFO         │
│  - SR / CPSR: status / prescaler            │
│  - IMSC / RIS / MIS / ICR: interrupt control│
│  - DMACR: DMA-request control (disabled)    │
│  - Default data path uses internal loopback │
└─────────────────────────────────────────────┘
```
The board layer is **not between the HAL and its register accesses**. It supplies configuration and invokes initialization.

## PL022 SSP Hardware Overview

### Key Features

The physical PL022 IP provides the following capabilities; they are not all exercised by this application or fully modeled in QEMU:

- **Motorola SPI protocol support** (Frame Format)
- **Master or Slave operation** (this driver initializes Master mode only)
- **4-16 bit data size** (this application and its byte-buffer transfer API use 8-bit data)
- **Programmable bit rate** via dual prescaler system
- **Integrated TX/RX FIFOs** (8 entries each)
- **Four SPI modes** (Mode 0-3 via CPOL/CPHA; the default application configures Mode 0)
- **Hardware loopback mode** for testing
- **Interrupt support** (TX/RX FIFO conditions, receive timeout, receive overrun)

### Register Map

| Offset | Register | Name | Description |
|--------|----------|------|-------------|
| 0x000 | CR0 | Control Register 0 | Data size, frame format, clock parameters |
| 0x004 | CR1 | Control Register 1 | SSP enable, master/slave, loopback |
| 0x008 | DR | Data Register | TX/RX FIFO access (write TX, read RX) |
| 0x00C | SR | Status Register | FIFO status, busy flag |
| 0x010 | CPSR | Clock Prescale | Even divider 2-254 |
| 0x014 | IMSC | Interrupt Mask | Gate peripheral interrupt sources; does not configure the NVIC |
| 0x018 | RIS | Raw Interrupt Status | Raw interrupt flags |
| 0x01C | MIS | Masked Interrupt Status | After masking |
| 0x020 | ICR | Interrupt Clear | Write 1 to clear receive overrun/timeout only; not RX/TX FIFO-level conditions |
| 0x024 | DMACR | DMA Control | Enable TX/RX DMA requests; requires a separately configured DMA system |

### Clock Generation

In Master mode, PL022 derives its serial output clock from its **SSPCLK input** using two divisors. Do not universally equate SSPCLK, the APB bus clock PCLK, and the Cortex-M3 core clock:

```
SSP_CLK = INPUT_CLOCK / (CPSDVSR × (SCR + 1))

Where:
  INPUT_CLOCK = SSPCLK (assumed to equal SystemCoreClock by this board layer)
  CPSDVSR = Clock prescale divisor (even 2-254) [CPSR register]
  SCR = Serial clock rate (0-255) [CR0[15:8]]
```

**Example** (this lab's default configuration):
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
1. CPSDVSR must be an even integer from 2 through 254
2. SCR can range from 0 through 255; its effective divisor is `SCR + 1`
3. Smaller divisor products produce faster output, with an arithmetic maximum of `SSPCLK / 2`
4. Search valid pairs to meet the target's maximum frequency; the current driver accepts explicit values and does not implement that search or validate these ranges


### SPI Mode Configuration (CR0 Register)

**Control Register 0 (CR0)** configures the SPI protocol parameters:

```
CR0 [31:0]:
  [15:8] SCR    - Serial Clock Rate (0-255)
  [7]    SPH    - SSPCLKOUT phase (CPHA)
  [6]    SPO    - SSPCLKOUT polarity (CPOL)
  [5:4]  FRF    - Frame format (00=SPI, 01=TI SSI, 10=Microwire; 11 reserved)
  [3:0]  DSS    - Data size select (0011=4-bit ... 1111=16-bit; 0000-0010 reserved)
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
  [3]  SOD  - Slave-mode output disable; not a Master-mode chip-select control
  [2]  MS   - Master/Slave select (0=Master, 1=Slave)
  [1]  SSE  - SSP Enable (1=Enabled)
  [0]  LBM  - Internal transmit-shifter to receive-shifter loopback
```

### Status Register (SR)

**SR** reports FIFO/busy status. Test the relevant bits rather than expecting a single whole-register constant:

```
SR [31:0]:
  [4] BSY  - SSP busy; includes queued TX data or an active serial transfer on hardware
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
    │  TX/RX serial shifters │
    └────────────────────────┘
            ↓         ↑
          MOSI      MISO
```

**FIFO Benefits**:
- **Batching**: A driver can service several entries per polling/interrupt visit
- **Throughput**: Keeping TX supplied and RX drained can reduce software-induced gaps
- **Interrupt efficiency**: PL022 asserts RX service at four or more entries and TX service at four or fewer entries, allowing a driver to service several entries per interrupt
- **Burst servicing**: Several accesses to DR are possible; each DR access handles one FIFO entry, not an eight-entry bulk transfer
- **Buffering**: FIFOs absorb limited service latency; they do not provide an external SPI receiver backpressure protocol

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
#define MPS2_SSP3 ((MPS2_SSP_TypeDef *)0x40026000UL)
```

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
cr0 |= ((uint32_t)scr << SSP_CR0_SCR_Pos);                // Clock rate
cr0 |= SSP_CR0_SPH_Msk;                         // Set CPHA bit

```

### 3. Polling vs Interrupt-Driven I/O

**Polling Pattern** (implemented in this lab):

**Advantages**:
- ✅ Simple synchronous control flow
- ✅ Straightforward status/return-code inspection
- ✅ No SSP ISR or NVIC enable is required for the polling transfer
- ✅ Small, directly inspectable byte-at-a-time implementation

**Disadvantages**:
- ❌ The caller actively polls instead of sleeping while waiting
- ❌ The call does not return until completion or a polling timeout
- ❌ No CPU-utilization or power reduction is demonstrated
- ❌ Interrupts/preemption may still occur; elapsed timing is not guaranteed

**Interrupt Pattern** (not implemented):

**Potential Advantages**:
- ✅ The application could do other work between service events
- ✅ Waiting could use sleep/completion mechanisms with suitable integration
- ✅ FIFO batching could reduce servicing overhead
- ✅ Multiple peripherals could be serviced through separate handlers

**Disadvantages**:
- ❌ ISR, NVIC routing/enabling, and completion state must be implemented
- ❌ Shared-state races and interrupt-source clearing require care
- ❌ Service latency can cause FIFO starvation, overrun, or inter-frame gaps
- ❌ Debugging must include asynchronous state, not just mask readback


### 4. Hardware Loopback Testing

Internal loopback routes the transmit serial-shifter output to the receive serial-shifter input. It does not require an external MOSI-to-MISO wire or a target device:

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
- ✅ Exercise controller TX/RX register paths without an external SPI target
- ✅ Compare received bytes with transmitted bytes
- ✅ Inspect configured clock-register values (not measure SCLK)
- ✅ Exercise basic FIFO availability/read/write behavior
- ✅ Run functional bring-up under QEMU without a physical prototype

### Basic Debugging

Exact breakpoint addresses and source-line numbers depend on the compiler and build. Set breakpoints by symbol instead of copying addresses from an earlier build:

```gdb
# Set breakpoints
(gdb) break main
(gdb) break debug_checkpoint

# Start execution
(gdb) continue
Breakpoint 1, main () at main.c:40

# Examine initial state
(gdb) print g_stage
$1 = 0

(gdb) print/x {tx_buffer[0], tx_buffer[1], tx_buffer[2], tx_buffer[3]}
$2 = {0x0, 0x0, 0x0, 0x0}
```

### Trace Through Stages

```gdb
# First checkpoint: buffers prepared, initialization not yet called
(gdb) continue
Breakpoint 2, debug_checkpoint () at main.c:24

(gdb) print g_stage
$3 = 1

# Snapshot variables have not been populated yet
(gdb) print/x {reg_cr0, reg_cr1, reg_cpsr}
$4 = {0x0, 0x0, 0x0}

# Second checkpoint: initialization complete, transfer not yet called
(gdb) continue
Breakpoint 2, debug_checkpoint () at main.c:24

(gdb) print g_stage
$5 = 2

(gdb) print init_result
$6 = 0  # MPS2_SSP_OK

# Examine the register snapshot captured after initialization
(gdb) print/x reg_cr0
$7 = 0x307  # SCR=3, FRF=0, SPO=0, SPH=0, DSS=7

(gdb) print/x reg_cr1
$8 = 0x3  # SSE=1, LBM=1, MS=0

(gdb) print/x reg_cpsr
$9 = 0x8  # CPSDVSR=8

(gdb) print g_board_ssp3.actual_clock_hz
$10 = 781250  # Software calculation: 25 MHz / (8 × 4)

(gdb) print/x {rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]}
$11 = {0x0, 0x0, 0x0, 0x0}  # Transfer has not started

# Third checkpoint: transfer and comparison complete
(gdb) continue
Breakpoint 2, debug_checkpoint () at main.c:24

(gdb) print g_stage
$12 = 3

(gdb) print transfer_result
$13 = 0  # MPS2_SSP_OK

# Verify loopback worked
(gdb) print/x {tx_buffer[0], tx_buffer[1], tx_buffer[2], tx_buffer[3]}
$14 = {0x9f, 0xa5, 0x5a, 0xff}

(gdb) print/x {rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3]}
$15 = {0x9f, 0xa5, 0x5a, 0xff}  # Matches TX

(gdb) print verify_result
$16 = 0  # Success

(gdb) print/x reg_imsc
$17 = 0x0  # All SSP interrupt sources are masked
```

### Examine Registers During Transfer

Use a fresh run for this section and set the transfer breakpoint before execution reaches `mps2_ssp_transfer()`. Do not inspect DR with a debugger expression: reading DR consumes an RX FIFO entry and changes program behavior.

```gdb
# Start from a newly launched or reset target
(gdb) break mps2_ssp_transfer
(gdb) continue
Breakpoint 1, mps2_ssp_transfer (...) at mps2_ssp.c:82

# Step through the TNF check to the first DR write
(gdb) next
(gdb) next

(gdb) print/x tx[i]
$1 = 0x9f

# The write on the current source line has not executed yet
(gdb) print/x ssp->regs->SR
$2 = 0x3  # TFE=1, TNF=1

# Execute the DR write. QEMU completes loopback immediately.
(gdb) next

(gdb) print/x ssp->regs->SR
$3 = 0x7  # TFE=1, TNF=1, RNE=1 in QEMU

# Pass the RNE check, then let firmware read DR
(gdb) next
(gdb) next

(gdb) print/x rx[0]
$4 = 0x9f
```

The exact intermediate SR value is timing-dependent on physical hardware. QEMU does not emulate the configured serial clock rate, so its TX-to-RX loopback completes during the MMIO write.

### Interrupt Configuration Test

This sequence continues from the first stage-3 checkpoint shown in **Trace Through Stages**:

```gdb
# Continue to the checkpoint after enabling RXIM and RTIM
(gdb) continue
Breakpoint 2, debug_checkpoint () at main.c:24

(gdb) print g_stage
$18 = 3

(gdb) print/x g_board_ssp3.regs->IMSC
$19 = 0x6  # RXIM=1, RTIM=1 (bits 2 and 1)

# Continue to the checkpoint after masking both sources again
(gdb) continue
Breakpoint 2, debug_checkpoint () at main.c:24

(gdb) print g_stage
$20 = 4

(gdb) print/x g_board_ssp3.regs->IMSC
$21 = 0x0  # All SSP interrupt sources are masked again
```

## Hardware vs Software SPI Comparison

### Lab 15 (Software Bit-Banging) vs Lab 16 (Hardware Controller)

| Aspect | Software (Lab 15) | Hardware (Lab 16) |
|--------|-------------------|-------------------|
| **Implementation** | Bit-level callbacks connected to a scripted RAM responder | PL022 MMIO connected to internal loopback |
| **Clock Speed** | Not measured | 781.25 kHz configured value; not timed by QEMU |
| **CPU Usage** | Synchronous CPU-driven loop | Synchronous polling; not benchmarked; no DMA transfer |
| **FIFO** | None | 8×16-bit TX/RX |
| **Interrupts** | No interrupt-driven transfer | Mask-register helpers only |
| **Flexibility** | Mode/order selection in bit algorithm | PL022 register capabilities |
| **Pins Used** | RAM fields, not physical GPIO | Default loopback needs no external target or pin wiring |
| **Multiple Buses** | Callback interface allows additional instances | Additional instances require board configuration and compatible mappings |

## Key Takeaways

### 1. Peripheral Programming Patterns

- Explain the initialization-time disable/configure/enable sequence without claiming glitch-free live reconfiguration
- Build CR0/CR1 values before their writes; distinguish these from CR1/IMSC read-modify-write expressions
- Use finite polling bounds, but do not confuse loop counts with time units
- Check TNF/RNE before FIFO access and use BSY when final physical-idle confirmation is required

### 2. Hardware Abstraction

- Separate application logic, board configuration, and a PL022-specific HAL
- Keep configuration structures distinct from runtime instances
- Select the correct base address in the board/SDK layer
- Use CMSIS qualifier conventions without assuming they enforce all hardware access rules

### 3. FIFO Management

- Check availability flags before data-register access
- A DR write enqueues TX data; a DR read consumes RX data
- FIFO batching is possible, but not implemented by the byte-at-a-time polling path
- Hardware receive overflow and pending/stale FIFO data require policies beyond this demonstration

### 4. ARM Cortex-M3 Features Used

- CPU load/store access to a memory-mapped peripheral block
- CMSIS-style register qualifiers and platform-specific register definitions
- Integer bit masking/shifting for register configuration
- Debug symbols and checkpoint functions for application inspection
- Peripheral interrupt-mask manipulation, **not** demonstrated NVIC interrupt delivery

## References

### ARM PrimeCell SSP (PL022)

- [PL022 Technical Reference Manual](https://developer.arm.com/documentation/ddi0194/latest/)

### ARM Cortex-M3

- [Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [Cortex-M3 Devices Generic User Guide](https://developer.arm.com/documentation/dui0552/latest/)

### CMSIS

- [CMSIS Documentation](https://arm-software.github.io/CMSIS_5/)
- [CMSIS Device Template](https://arm-software.github.io/CMSIS_5/Core/html/device_h_pg.html)

### MPS2+ Platform

- [Cortex-M Prototyping System (MPS2+) data sheet](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Development%20boards%20datasheets/Datasheet_V2M-MPS2plus.pdf?hash=0AF46F54F59EF1D1991D2EA27B2F4B3D062F4539&revision=12ddc10b-7e47-4cc5-a3d2-e0aa34c061da)
- [AN385 - ARM Cortex-M3 SMM on V2M-MPS2](https://developer.arm.com/documentation/dai0385/latest/)

### SPI Protocol

- [SPI Protocol Wikipedia](https://en.wikipedia.org/wiki/Serial_Peripheral_Interface)
- [M68HC11E Family Data Sheet, Serial Peripheral Interface chapter](https://www.nxp.com/docs/en/data-sheet/MC68HC11E.pdf)

### Embedded Systems

- [Making Embedded Systems by Elecia White](https://www.oreilly.com/library/view/making-embedded-systems/9781449308889/)
- [Embedded Software Primer by David E. Simon](https://www.pearson.com/en-us/subject-catalog/p/embedded-software-primer/P200000003312)
