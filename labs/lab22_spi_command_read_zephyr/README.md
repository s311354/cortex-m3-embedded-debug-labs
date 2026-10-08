# Lab 22: SPI Command-Read Protocol with Zephyr RTOS

## Overview

This lab demonstrates **SPI command-response transactions using the Zephyr RTOS driver API** with the ARM PrimeCell PL022 SSP controller on the MPS2/AN385 platform. Unlike the bare-metal Labs 15 and 16, this exercise integrates with **Zephyr's device model, device tree infrastructure, and standardized SPI API**. The lab implements a **command-read helper abstraction** suitable for SPI flash memory or sensor communication patterns, validates the driver stack through internal loopback mode, and exercises comprehensive error handling.

The lab runs on QEMU's `mps2-an385` machine using **SSP3 / Shield0 at `0x40026000`** with **8-bit transfers, MSB-first, Mode 0, and internal loopback**. It demonstrates production-quality embedded development practices: device tree hardware description, custom Zephyr drivers, compile-time assertions, structured error handling, and GDB-friendly debugging patterns.

## Learning Objectives

### Zephyr RTOS Integration
- Understand Zephyr's device model and driver lifecycle
- Use device tree (DTS) for hardware configuration
- Access peripherals through Zephyr's standard driver APIs
- Implement custom clock control drivers for peripheral requirements
- Navigate Zephyr's build system (CMake + Kconfig + DTS)

### SPI Command-Response Protocol
- Implement SPI command-read transactions (common for flash/sensors)
- Understand two-phase SPI transfers: command byte + dummy clocks
- Use buffer sets for split TX/RX operations
- Handle variable-length responses within size constraints

### Cortex-M3 and PL022 Hardware
- Program the ARM PrimeCell PL022 SSP via Zephyr abstractions
- Understand peripheral memory mapping (MMIO at `0x40026000`)
- Access configuration registers for post-transfer verification
- Map NVIC interrupt lines through device tree (IRQ 24)


## Architecture Overview

```
┌────────────────────────────────────────────────┐
│  Application Layer (main.c)                    │
│  - Test orchestration and staged validation    │
│  - Checkpoint functions for GDB debugging      │
│  - Register snapshot collection                │
│  - Buffer management and verification          │
└──────────────┬─────────────────────────────────┘
               │ uses
               ▼
┌────────────────────────────────────────────────┐
│  Protocol Layer (spi_command.c/h)              │
│  - spi_client abstraction (bus + config)       │
│  - spi_client_ready() configuration validation │
│  - spi_command_read() command-response helper  │
│  - Argument validation and error codes         │
│  - Buffer set construction for split TX/RX     │
└──────────────┬─────────────────────────────────┘
               │ calls
               ▼
┌────────────────────────────────────────────────┐
│  Zephyr SPI Driver API (drivers/spi/)          │
│  - spi_transceive() synchronous transfer       │
│  - device_is_ready() device state checking     │
│  - SPI operation mode flags and validation     │
└──────────────┬─────────────────────────────────┘
               │ configures
               ▼
┌────────────────────────────────────────────────┐
│  PL022 Driver (drivers/spi/spi_pl022.c)        │
│  - Register configuration from spi_config      │
│  - FIFO-based polling transfers                │
│  - Clock rate calculation from clock control   │
│  - Pinctrl integration for GPIO muxing         │
└──────────────┬─────────────────────────────────┘
               │ uses
               ▼
┌────────────────────────────────────────────────┐
│  Clock Control Driver (lab22_fixed_clock.c)    │
│  - Custom fixed-rate clock provider            │
│  - Adapts to PL022's clock control requirement │
│  - Provides 25 MHz reference clock             │
└──────────────┬─────────────────────────────────┘
               │ all layers access
               ▼
┌────────────────────────────────────────────────┐
│  Device Tree (app.overlay)                     │
│  - Hardware topology: SPI bus @ 0x40026000     │
│  - Target device configuration                 │
│  - Clock and interrupt bindings                │
│  - Pinctrl assignments                         │
└────────────────────────────────────────────────┘
               │ describes
               ▼
┌────────────────────────────────────────────────┐
│  PL022 Hardware Registers (0x40026000)         │
│  - CR0/CR1: configuration and control          │
│  - DR: TX/RX FIFO data register                │
│  - SR: status (FIFO full/empty, busy)          │
│  - CPSR: clock prescale                        │
│  - IMSC/RIS/MIS/ICR: interrupt management      │
└────────────────────────────────────────────────┘
```

## Device Tree Configuration

### Key Device Tree Concepts

| Concept | Implementation |
|---------|----------------|
| **Custom Clock Provider** | `lab22,pl022-clock` with fixed 25 MHz rate |
| **PL022 Bus Node** | `spi@40026000` with standard `arm,pl022` compatible |
| **Target Device** | `target@0` represents the SPI peripheral (chip select 0) |
| **Clock Cells** | `#clock-cells = <1>` allows clock consumer to specify clock ID |
| **Interrupt Mapping** | `interrupts = <24 3>` routes to NVIC IRQ 24 |
| **Register Address** | `reg = <0x40026000 0x1000>` maps to MPS2 Shield0 SSP3 |

### Custom Device Tree Bindings

The lab provides two custom bindings in `dts/bindings/`:

**`clock/lab22,pl022-clock.yaml`**
**`spi/lab22,spi-target.yaml`**

These bindings enable compile-time validation of device tree structure and generate macros for C code access.

## Custom Clock Control Driver

### Why a Custom Clock Driver?

Zephyr v4.2.0's PL022 driver requires a clock control phandle with a clock ID cell. The MPS2 `sysclk` binding has zero clock cells, creating a mismatch. The `lab22_fixed_clock.c` driver bridges this gap.

### Clock Control API

| Function | Purpose |
|----------|---------|
| `on()` | Enable clock (no-op for fixed clock) |
| `off()` | Disable clock (no-op for fixed clock) |
| `get_rate()` | Return configured frequency (25 MHz) |
| `get_status()` | Always returns `CLOCK_CONTROL_STATUS_ON` |

The PL022 driver queries this rate to calculate prescaler values for the requested SPI frequency.

## SPI Command-Read Protocol

### Protocol Pattern

The command-read pattern is ubiquitous in SPI devices:

```
Phase 1: Master → Command Byte    Slave → Don't Care
Phase 2: Master → Dummy Bytes      Slave → Response Data
```

**Examples:**
- **SPI Flash:** `0x9F` (Read JEDEC ID) → 3 bytes (Manufacturer, Type, Capacity)
- **Sensor:** `0x0F` (WHO_AM_I) → 1 byte (device ID)
- **ADC:** `0x00` (start conversion) → 2 bytes (ADC value)

### Validation Logic

**`spi_client_ready()` checks:**

- Valid pointers (client, bus, config)
- Non-zero frequency
- Supported operations (8-bit, controller mode, full duplex)
- Device readiness (`device_is_ready()`)
- GPIO chip select configuration (if used)
- GPIO polarity vs. SPI CS polarity consistency

**`spi_command_read()` checks:**

- Non-NULL reply buffer
- Non-zero reply length
- Reply length ≤ `SPI_COMMAND_MAX_REPLY` (16 bytes)
- Client validation via `spi_client_ready()`

### Error Codes

| Code | Meaning | Trigger |
|------|---------|---------|
| `-EINVAL` | Invalid argument | NULL pointer, zero length, config mismatch |
| `-EMSGSIZE` | Message too large | `reply_len > SPI_COMMAND_MAX_REPLY` |
| `-ENODEV` | Device not ready | Bus or GPIO CS not initialized |
| `-ENOTSUP` | Not supported | Wrong mode, half-duplex, TI frame format |

## PL022 SSP Register Details

### Control Register 0 (CR0) @ 0x000

| Bits | Field | Description |
|------|-------|-------------|
| 15:8 | SCR | Serial clock rate divider: SSP_CLK = SSPCLK / (CPSDVSR × (SCR+1)) |
| 7 | SPH | Clock phase (CPHA): 0 = capture on first edge, 1 = second edge |
| 6 | SPO | Clock polarity (CPOL): 0 = low idle, 1 = high idle |
| 5:4 | FRF | Frame format: 00 = Motorola SPI |
| 3:0 | DSS | Data size: 0111 = 8 bits, 1111 = 16 bits |

**Lab configuration:** `0x0007` → 8-bit data, SPI mode 0 (CPOL=0, CPHA=0), SCR=0

### Control Register 1 (CR1) @ 0x004

| Bits | Field | Description |
|------|-------|-------------|
| 3 | SOD | Slave output disable (slave mode only) |
| 2 | MS | Master/Slave: 0 = master, 1 = slave |
| 1 | SSE | SSP enable: 1 = peripheral enabled |
| 0 | LBM | Loopback mode: 1 = internal loopback |

**Lab configuration:** `0x0003` → Loopback enabled, SSP enabled, master mode

### Status Register (SR) @ 0x00C

| Bits | Field | Description |
|------|-------|-------------|
| 4 | BSY | Busy: 1 = transmitting/receiving |
| 3 | RFF | RX FIFO full |
| 2 | RNE | RX FIFO not empty |
| 1 | TNF | TX FIFO not full |
| 0 | TFE | TX FIFO empty |

**Polling sequence:**
1. Wait for TNF=1 before writing to DR
2. Wait for RNE=1 before reading from DR
3. Wait for BSY=0 after last byte

### Clock Prescale Register (CPSR) @ 0x010

| Bits | Field | Description |
|------|-------|-------------|
| 7:0 | CPSDVSR | Clock prescale divisor (must be even, 2-254) |

**Calculation example:**
- Input clock: 25 MHz
- Requested SPI clock: 781.25 kHz
- CPSR = 2, SCR = 15
- Actual: 25000000 / (2 × 16) = 781250 Hz ✓

### Data Register (DR) @ 0x008

| Bits | Field | Description |
|------|-------|-------------|
| 15:0 | DATA | TX/RX data (right-justified for <16-bit transfers) |

**Access pattern:**
- Write: Push byte to TX FIFO (wait for TNF=1 first)
- Read: Pop byte from RX FIFO (wait for RNE=1 first)

## Comparison with Bare-Metal Labs

### Lab 15: Software SPI (Bit-Banging)

| Aspect | Lab 15 | Lab 22 |
|--------|--------|--------|
| **Hardware** | GPIO bit-banging | PL022 hardware controller |
| **Timing** | Software loop delays | Hardware clock generation |
| **FIFOs** | None | 8-entry TX/RX FIFOs |
| **RTOS** | Bare metal | Zephyr RTOS |
| **Configuration** | Code constants | Device tree |

### Lab 16: Hardware SPI (Bare-Metal PL022)

| Aspect | Lab 16 | Lab 22 |
|--------|--------|--------|
| **Platform** | Bare metal | Zephyr RTOS |
| **Driver** | Custom HAL | Zephyr `spi_pl022.c` |
| **Hardware Description** | C header defines | Device tree overlay |
| **API Level** | Direct register access | Standard `drivers/spi` API |
| **Protocol** | Raw byte transfers | Command-read abstraction |
| **Error Handling** | Return codes | POSIX errno conventions |
| **Startup** | Custom linker + startup.s | Zephyr runtime |

**Key Evolution:**

```
Lab 15 → Lab 16 → Lab 22
GPIO   → PL022   → PL022 + Zephyr
       → HAL     → Standard API
       → MMIO    → Device Tree
```


## Production Development Practices

### Layered Architecture
```
Application
    ↓ uses
Protocol Abstraction (spi_command_read)
    ↓ uses
Standard Driver API (spi_transceive)
    ↓ controls
Hardware (PL022 registers)
```

Each layer has clear responsibility and testable interface.

### Compile-Time Validation
```c
BUILD_ASSERT(DT_REG_ADDR(BUS_NODE) == 0x40026000U, 
             "Expected Shield0 mapping");
BUILD_ASSERT(SPI_COMMAND_MAX_REPLY >= SHORT_REPLY, 
             "Reply limit too small");
```

Catches configuration errors before runtime.

### Error Handling Strategy
- **Input validation** at API boundaries
- **POSIX error codes** for portability
- **Defensive checks** with early return
- **Fail-fast** on unrecoverable errors

### Separation of Concerns
- **Hardware description** in device tree
- **Driver code** in Zephyr subsystem
- **Protocol logic** in helper layer
- **Application** in main.c
- **Build configuration** in prj.conf/CMakeLists.txt

## Key Takeaways

1. **Zephyr abstracts hardware while preserving visibility**: Device tree documents hardware, but you can still access registers directly

2. **Custom drivers integrate seamlessly**: The clock control driver demonstrates how to extend Zephyr for project-specific needs

3. **Protocol abstractions improve code reuse**: `spi_command_read()` captures common SPI pattern, reusable across sensors/flash devices

4. **Validation prevents subtle bugs**: Compile-time assertions + runtime checks catch configuration errors early

5. **GDB-friendly patterns aid debugging**: Volatile globals + checkpoint functions make embedded debugging practical

6. **RTOS adds structure, not just multitasking**: Device model, driver lifecycle, power management (even in single-threaded app)

7. **Device tree is hardware DSL**: Separating topology from code improves portability and compile-time checking


## Related Documentation

- ARM PrimeCell SSP (PL022) Technical Reference Manual
- Zephyr SPI Driver API Documentation
- Zephyr Device Tree Guide
- MPS2/AN385 Technical Reference Manual
