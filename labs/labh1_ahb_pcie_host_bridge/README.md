# Lab H1: AHB PCIe Host Bridge Driver

## Overview

This lab demonstrates **hardware/software co-design** for ARM Cortex-M3 by implementing a PCIe host controller driver that communicates with custom RTL hardware through the AHB bus. Unlike basic peripheral labs, this includes both the software driver (C) and hardware implementation (Verilog RTL).

## Learning Objectives

### Hardware/Software Interface
- Custom peripheral integration with Cortex-M3 via AHB-Lite bus
- Memory-mapped register interface design
- Command/status register patterns for complex transactions

### Driver Development
- BSP-level driver architecture for custom SoCs
- Polling-based hardware synchronization
- Error handling for hardware operations
- Compile-time register layout verification

### PCIe Fundamentals
- PCIe configuration space access
- Bus/Device/Function (BDF) addressing
- Host bridge transaction model

## Hardware Architecture

```text
┌─────────────────┐
│  Cortex-M3 CPU  │
└────────┬────────┘
         │ AHB-Lite Bus
         ▼
┌────────────────────────────────────────┐
│  labh1_ahb_pcie_host_bridge (Verilog)  │
│                                         │
│  ┌──────────────┐   ┌──────────────┐  │
│  │ AHB Slave    │   │ Request FSM  │  │
│  │ Interface    ├──►│ (IDLE/ISSUE/ │  │
│  │ (Registers)  │   │  WAIT_CPL)   │  │
│  └──────────────┘   └──────┬───────┘  │
│                             │           │
│                             ▼           │
│                      PCIe Backend       │
└─────────────────────────┬───────────────┘
                          │
                          ▼
                  PCIe Endpoint Device
                  (Vendor: 0x1234, Device: 0x5678)
```

### Memory Map

| Base Address  | Size    | Description                |
|---------------|---------|----------------------------|
| `0xA0000000` | 64 KB   | PCIe Host Bridge Registers |

### Register Layout

| Offset | Name          | Access | Description                           |
|--------|---------------|--------|---------------------------------------|
| 0x00   | VERSION       | RO     | Hardware version (0x00010000)         |
| 0x04   | CONTROL       | RW     | Control register (bit 0: ENABLE)      |
| 0x08   | STATUS        | RO     | Status flags (BUSY/DONE/ERROR/BUS)    |
| 0x0C   | (reserved)    | -      | Reserved                              |
| 0x10   | CFG_BDF       | RW     | Target Bus/Device/Function            |
| 0x14   | CFG_REG       | RW     | Configuration register offset         |
| 0x18   | CFG_WDATA     | RW     | Write data for config writes          |
| 0x1C   | CFG_RDATA     | RO     | Read data from config reads           |
| 0x20   | CFG_COMMAND   | RW     | Command register (doorbell)           |
| 0x24   | ERROR_STATUS  | RO     | Detailed error information            |

## Key Concepts

### 1. AHB-Lite Bus Protocol

The Cortex-M3 accesses the PCIe bridge through the AHB-Lite bus:

```verilog
// AHB-Lite slave interface signals
input wire         HSEL,      // Peripheral select
input wire         HREADY,    // Previous transfer complete
input wire [31:0]  HADDR,     // Address bus
input wire [1:0]   HTRANS,    // Transfer type
input wire         HWRITE,    // Write enable
input wire [31:0]  HWDATA,    // Write data
output reg [31:0]  HRDATA,    // Read data
output wire        HREADYOUT, // Transfer complete
output wire        HRESP      // Response (OK/ERROR)
```

### 2. Structured MMIO with Compile-Time Checks

```c
struct labh1_pcie_regs {
    volatile uint32_t version;
    volatile uint32_t control;
    volatile uint32_t status;
    uint32_t reserved0;
    volatile uint32_t cfg_bdf;
    // ...
};

// Verify register layout at compile time
_Static_assert(
    offsetof(struct labh1_pcie_regs, cfg_bdf) == 0x10U,
    "CFG_BDF offset mismatch"
);
```

This ensures the C struct matches the hardware register map.

### 3. Command/Status Transaction Pattern

The bridge uses a "doorbell" pattern:

1. **Setup**: Write parameters to `cfg_bdf`, `cfg_reg`, optionally `cfg_wdata`
2. **Trigger**: Write command to `cfg_command` (doorbell register)
3. **Poll**: Read `status` register until `DONE` bit is set
4. **Result**: Check `ERROR` bit, read `cfg_rdata` if config read

```c
g_pcie_regs->cfg_bdf = bdf;
g_pcie_regs->cfg_reg = reg;
g_pcie_regs->cfg_command = LABH1_CMD_CFG_READ;  // Doorbell!

// Poll for completion
while (!(g_pcie_regs->status & LABH1_STATUS_DONE)) {
    // Wait...
}
```

### 4. PCIe BDF Addressing

PCIe devices are identified by Bus/Device/Function:

```c
#define LABH1_PCIE_BDF(bus, dev, fn) \
    ((((uint32_t)(bus) & 0xFFU) << 16U) | \
     (((uint32_t)(dev) & 0x1FU) << 11U) | \
     (((uint32_t)(fn)  & 0x07U) << 8U))

uint32_t endpoint = LABH1_PCIE_BDF(0, 1, 0);  // Bus 0, Device 1, Function 0
```

### 5. Polling with Timeout

Hardware can fail or hang. Always use timeouts:

```c
#define LABH1_POLL_LIMIT 100000U
```

## Code Structure

### Driver Layer (`pcie_ahb_host.c/h`)

**Functions:**
- `labh1_pcie_version()` - Read hardware version
- `labh1_pcie_host_init()` - Initialize and enable bridge
- `labh1_pcie_config_read32()` - Read from PCIe config space
- `labh1_pcie_config_write32()` - Write to PCIe config space

### Hardware Layer (`rtl/`)

**Modules:**
- `labh1_ahb_pcie_host_bridge.v` - Main AHB slave and FSM
- `labh1_pcie_backend_stub.v` - Simulated PCIe backend
- `m3ds_pcie_host_wrapper.v` - Integration wrapper

**State Machine:**
1. `STATE_IDLE` - Waiting for command
2. `STATE_ISSUE` - Issuing request to PCIe backend
3. `STATE_WAIT_CPL` - Waiting for completion

## Test Flow

The `main.c` program performs a simple PCIe device enumeration:

```text
Stage 1: Initialize
├─ Read hardware version
├─ Check version matches expected value
└─ Enable PCIe host bridge

Stage 2: Read Configuration Space
├─ Target endpoint at BDF 0:1.0
├─ Read offset 0x00 (Vendor ID / Device ID)
└─ Store 32-bit value

Stage 3: Validate Device Identity
├─ Extract Vendor ID (lower 16 bits)
├─ Extract Device ID (upper 16 bits)
├─ Verify Vendor ID = 0x1234
└─ Verify Device ID = 0x5678

Result: Success (g_stage = 3) or Failure (g_stage = 0xFFFFFFFF)
```

### Debug Checkpoints

The code includes `labh1_debug_checkpoint()` functions - these are no-op functions that serve as breakpoints for GDB:

```bash
(gdb) break labh1_debug_checkpoint
(gdb) continue
# Inspect g_stage, g_version, g_init_result, etc.
```

## Debugging with GDB

```bash
# Terminal 1: Start QEMU (if custom device supported)
qemu-system-arm -M mps2-an385 -cpu cortex-m3 \
    -nographic -semihosting \
    -kernel build/labh1.elf \
    -s -S

# Terminal 2: Connect GDB
arm-none-eabi-gdb build/labh1.elf
(gdb) target remote :1234
(gdb) break labh1_debug_checkpoint
(gdb) continue

# Check initialization
(gdb) print g_stage
(gdb) print/x g_version
(gdb) print g_init_result

# Continue to next checkpoint
(gdb) continue

# Check configuration read results
(gdb) print/x g_endpoint_bdf
(gdb) print/x g_id
(gdb) print/x g_vendor
(gdb) print/x g_device
```

## Expected Results

### Successful Execution

```
g_stage = 1
g_version = 0x00010000
g_init_result = 0 (LABH1_OK)

g_stage = 2
g_endpoint_bdf = 0x00000800  (Bus 0, Device 1, Function 0)
g_cfg_result = 0 (LABH1_OK)
g_id = 0x56781234

g_stage = 3
g_vendor = 0x1234
g_device = 0x5678
```

## Cortex-M3 Specific Features

### Memory Protection Unit (MPU) Considerations

For production use, configure MPU for device memory:

```c
// Region: 0xA0000000 - 0xA0010000 as Device memory
// Attributes: Non-cacheable, non-bufferable
```
## References
- **ARM IHI 0033B** - AMBA AHB-Lite Protocol Specification
- **ARM DDI 0337I** - Cortex-M3 Technical Reference Manual
- **PCI Express Base Specification** - PCIe configuration space
- **CMSIS-Core** - Cortex-M core register access

## Summary

This lab bridges the gap between simple peripheral drivers and complex SoC development:

- **Hardware designers** learn how software accesses their IP blocks
- **Software engineers** understand the hardware constraints and protocols
- **BSP developers** see realistic driver patterns for custom peripherals

The combination of C driver code and Verilog RTL makes this a complete hardware/software co-design example for ARM Cortex-M3 systems.
