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

## Automated Verification Workflow

The `verify.sh` script provides a complete verification flow from simulation to waveform analysis:

### Verification Steps

```bash
./verify.sh [--wave]
```

**Step 1: Clean Build** - Removes previous simulation artifacts

**Step 2: Run RTL Simulation** - Executes `make sim` and captures output log

**Step 3: Verify Results** - Checks simulation log for:
- Test pass/fail status ("LABH1 PASS" string)
- Correct device ID returned (0x56781234)
- Vendor ID extraction (0x1234)
- Device ID extraction (0x5678)

**Step 4: Check VCD File** - Confirms waveform dump was generated at `build/labh1.vcd`

### Output Example

```text
===================================================
Lab H1 Hardware Verification Script
===================================================

[1/4] Cleaning previous build...
      ✓ Clean complete

[2/4] Running RTL simulation...
      ✓ Simulation complete

[3/4] Verifying results...
      ✓ PASS: LABH1 PASS: g_id = 0x56781234
      ✓ Correct device ID (0x56781234)
        - Vendor: 0x1234
        - Device: 0x5678

[4/4] Checking waveform file...
      ✓ VCD file generated: build/labh1.vcd (123456 bytes)

===================================================
✓ ALL VERIFICATION CHECKS PASSED
===================================================

To view waveforms, run:
  gtkwave build/labh1.vcd labh1_verification.gtkw

Or re-run with: ./verify.sh --wave
```

### GTKWave Integration

The `--wave` flag automatically launches GTKWave after successful verification:

```bash
# Automatic launch after verification
./verify.sh --wave
```

**Behavior:**
1. Checks if `gtkwave` is installed
2. Looks for pre-configured save file `labh1_verification.gtkw`
3. Launches GTKWave in background with saved signal configuration
4. If save file missing, opens raw VCD file

## Hardware Verification with GTKWave

After running the RTL simulation, you can inspect the waveforms to verify hardware behavior at the signal level. This is essential for hardware/software co-design debugging.

### Quick Start

```bash
# Run simulation and open waveforms automatically
./verify.sh --wave

# Or manually after simulation
gtkwave build/labh1.vcd labh1_verification.gtkw
```

### Pre-Configured Signal Groups

The `labh1_verification.gtkw` save file organizes signals into logical groups:

#### 1. **AHB Interface Signals**
Monitor the AHB-Lite bus transactions between Cortex-M3 and the bridge:

| Signal | Description | What to Verify |
|--------|-------------|----------------|
| `HCLK` | System clock | Clock toggles at expected frequency |
| `HRESETn` | Active-low reset | Goes high after reset sequence |
| `HSEL` | Bridge selected | Asserts when accessing 0xA0000000 range |
| `HADDR[31:0]` | Address bus | Shows register offsets (0x00, 0x04, 0x10, etc.) |
| `HTRANS[1:0]` | Transfer type | `2'b10` (NONSEQ) or `2'b11` (SEQ) |
| `HWRITE` | Write enable | `1` for writes, `0` for reads |
| `HWDATA[31:0]` | Write data | Data written to registers |
| `HRDATA[31:0]` | Read data | Data read from registers |
| `HREADYOUT` | Bridge ready | `0` indicates wait states |

**Key Transaction Pattern:**
```
1. HADDR shows register address
2. HTRANS = NONSEQ (new transaction)
3. HWRITE indicates read or write
4. HREADYOUT may insert wait states
5. HWDATA (write) or HRDATA (read) carries data
```

#### 2. **Bridge Internal Registers**
Verify register state changes during PCIe transactions:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `control[31:0]` | Control register | Bit 0 set to 1 after initialization |
| `busy` | Transaction in progress | Asserts during FSM operation |
| `done` | Transaction complete | Asserts when FSM returns to IDLE |
| `cfg_bdf[31:0]` | Target BDF | Should be 0x00000800 (Bus 0, Dev 1, Fn 0) |
| `cfg_reg[31:0]` | Config offset | Should be 0x00000000 (offset 0) |
| `cfg_command[31:0]` | Command doorbell | `1` for config read, `2` for config write |
| `cfg_rdata[31:0]` | Read data result | Should contain 0x56781234 after read |
| `error_status[31:0]` | Error flags | Should remain 0 for successful operation |

#### 3. **FSM State Tracking**
The state machine progresses through three states:

| State Value | State Name | Description |
|-------------|------------|-------------|
| `2'b00` | `STATE_IDLE` | Waiting for command in `cfg_command` |
| `2'b01` | `STATE_ISSUE` | Issuing request to PCIe backend |
| `2'b10` | `STATE_WAIT_CPL` | Waiting for completion from backend |

**Expected Sequence:**
```
IDLE → (command written) → ISSUE → WAIT_CPL → (completion received) → IDLE
```

#### 4. **Backend Request Interface**
Request handshake to PCIe backend:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `req_valid` | Request valid | Asserts in ISSUE state |
| `req_ready` | Backend ready | Backend acknowledges request |
| `req_type[1:0]` | Request type | `0` = read, `1` = write |
| `req_bdf[31:0]` | Target BDF | 0x00000800 for test endpoint |
| `req_reg[9:0]` | Register offset | 0x000 for vendor/device ID |
| `req_wdata[31:0]` | Write data | (not used for read transactions) |

**Handshake Protocol:**
- Both `req_valid` and `req_ready` must be high for one cycle
- Transaction completes when handshake occurs

#### 5. **Backend Completion Interface**
Completion handshake from PCIe backend:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `cpl_valid` | Completion valid | Backend asserts when done |
| `cpl_ready` | Bridge ready | Bridge asserts in WAIT_CPL state |
| `cpl_status[1:0]` | Completion status | `0` = success, non-zero = error |
| `cpl_rdata[31:0]` | Read data | 0x56781234 for vendor/device ID read |

#### 6. **Backend Stub Internals**
Simulated PCIe backend behavior:

| Signal | Description | Notes |
|--------|-------------|-------|
| `pending` | Request pending | Set when request received |
| `delay_count[1:0]` | Simulation delay | Counts down to simulate PCIe latency |
| `saved_*` | Saved request | Stores request parameters during processing |

### Verification Checklist

Use GTKWave to verify these key events in sequence:

#### ✓ **Initialization (Early in simulation)**
- [ ] `control[0]` transitions from 0 to 1 (ENABLE bit set)
- [ ] `HRESETn` is high (system out of reset)
- [ ] No error flags set in `error_status`

#### ✓ **Configuration Read Setup**
- [ ] AHB write to offset 0x10 sets `cfg_bdf` = 0x00000800
- [ ] AHB write to offset 0x14 sets `cfg_reg` = 0x00000000
- [ ] AHB write to offset 0x20 sets `cfg_command` = 0x00000001 (doorbell)

#### ✓ **FSM Transaction**
- [ ] `state` transitions: IDLE (00) → ISSUE (01) → WAIT_CPL (10) → IDLE (00)
- [ ] `busy` asserts during ISSUE and WAIT_CPL states
- [ ] `done` asserts when returning to IDLE

#### ✓ **Backend Handshakes**
- [ ] `req_valid` and `req_ready` both high for one cycle (request accepted)
- [ ] `cpl_valid` and `cpl_ready` both high for one cycle (completion delivered)
- [ ] `cpl_status` = 0 (successful completion)

#### ✓ **Result Validation**
- [ ] `cfg_rdata` contains 0x56781234 after completion
- [ ] AHB read from offset 0x1C returns 0x56781234 to software
- [ ] `done` bit visible in STATUS register (offset 0x08)

### Extracting Timing Information

To measure transaction latency:

1. Find rising edge of `cfg_command` write (doorbell trigger)
2. Find corresponding `done` bit assertion in `status` register
3. Calculate cycles: `(done_time - command_time) / clock_period`

Expected latency: ~5-10 cycles (depends on backend stub delay)

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
