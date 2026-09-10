# Lab H2: PCIe Transaction Layer Packet (TLP) Model

## Overview

This lab demonstrates **PCIe Transaction Layer Protocol implementation** for ARM Cortex-M3 by extending Lab H1's host bridge with a realistic TLP packet processing model. While Lab H1 used a simplified backend stub, Lab H2 implements actual PCIe packet formatting, transmission, reception, and endpoint device simulation with transaction-layer latency modeling.

## Learning Objectives

### PCIe Transaction Layer Protocol
- Transaction Layer Packet (TLP) structure and encoding
- Configuration read/write TLP formatting
- Completion TLP decoding and status handling
- Transaction-layer latency and handshake protocols

### Hardware/Software Co-Design
- Multi-module hardware architecture with packet processing pipeline
- Request/completion handshake protocols
- Realistic endpoint device simulation
- Absent device detection and enumeration behavior

### Driver Development Patterns
- Read-modify-write sequences for configuration registers
- Configuration space verification and validation
- Error handling for transaction failures
- Polling with timeout for asynchronous hardware operations

### Verification Methodology
- Unit testing of individual TLP modules
- End-to-end system integration testing
- Waveform analysis of packet flows
- Software/hardware interface validation

## Hardware Architecture

```text
┌─────────────────┐
│  Cortex-M3 CPU  │
└────────┬────────┘
         │ AHB-Lite Bus (HADDR, HDATA, HWRITE, etc.)
         ▼
┌────────────────────────────────────────────────────────────┐
│  labh1_ahb_pcie_host_bridge                                │
│  (from Lab H1 - handles AHB protocol & FSM)                │
└────────┬───────────────────────────────────────────────────┘
         │ Request/Completion Interface
         ▼
┌────────────────────────────────────────────────────────────┐
│  m3ds_pcie_backend (Lab H2 - TLP Processing Pipeline)      │
│                                                             │
│  ┌──────────────────┐   ┌──────────────────────────────┐  │
│  │ labh2_pcie_tlp_tx│   │ labh2_pcie_endpoint_model    │  │
│  │                  │   │                              │  │
│  │ Request → TLP    ├──►│ ┌────────────────────────┐  │  │
│  │ Encoder          │   │ │ Config Space Registers │  │  │
│  │                  │   │ │ • Vendor ID: 0x1234    │  │  │
│  │ • DW0: Opcode    │   │ │ • Device ID: 0x5678    │  │  │
│  │ • DW1: BDF       │   │ │ • Scratch: 0x040       │  │  │
│  │ • DW2: Write Data│   │ │                        │  │  │
│  │ • DW3: Reserved  │   │ │ Transaction Latency:   │  │  │
│  └──────────────────┘   │ │ 2-cycle delay model    │  │  │
│                          │ └────────────────────────┘  │  │
│  ┌──────────────────┐   │                              │  │
│  │ labh2_pcie_tlp_rx│◄──┤ Completion Generator         │  │
│  │                  │   │                              │  │
│  │ TLP → Completion │   └──────────────────────────────┘  │
│  │ Decoder          │                                      │
│  │                  │   PCIe Endpoint Device               │
│  │ • Status Extract │   (BDF: 00:01.0)                    │
│  │ • Data Extract   │                                      │
│  └──────────────────┘                                      │
│                                                             │
│  TLP Transaction Layer Packet Protocol                     │
└─────────────────────────────────────────────────────────────┘
```

### Memory Map

| Base Address  | Size    | Description                |
|---------------|---------|----------------------------|
| `0xA0000000` | 64 KB   | PCIe Host Bridge Registers |

### Register Layout

Same as Lab H1 - the software interface is preserved:

| Offset | Name          | Access | Description                           |
|--------|---------------|--------|---------------------------------------|
| 0x00   | VERSION       | RO     | Hardware version (0x00010000)         |
| 0x04   | CONTROL       | RW     | Control register (bit 0: ENABLE)      |
| 0x08   | STATUS        | RO     | Status flags (BUSY/DONE/ERROR)        |
| 0x0C   | (reserved)    | -      | Reserved                              |
| 0x10   | CFG_BDF       | RW     | Target Bus/Device/Function            |
| 0x14   | CFG_REG       | RW     | Configuration register offset         |
| 0x18   | CFG_WDATA     | RW     | Write data for config writes          |
| 0x1C   | CFG_RDATA     | RO     | Read data from config reads           |
| 0x20   | CFG_COMMAND   | RW     | Command register (doorbell)           |
| 0x24   | ERROR_STATUS  | RO     | Detailed error information            |

## Key Concepts

### 1. PCIe Transaction Layer Packet (TLP) Structure

Lab H2 models PCIe TLPs as multi-DWORD packets:

**Configuration Read TLP:**
```
DW0: [31:24] Opcode = 0x01 (CFG_READ)
     [23:10] Reserved
     [9:0]   Register offset
DW1: [31:0]  BDF (Bus/Device/Function)
DW2: [31:0]  Reserved
DW3: [31:0]  Reserved
```

**Configuration Write TLP:**
```
DW0: [31:24] Opcode = 0x02 (CFG_WRITE)
     [23:10] Reserved
     [9:0]   Register offset
DW1: [31:0]  BDF (Bus/Device/Function)
DW2: [31:0]  Write Data
DW3: [31:0]  Reserved
```

**Completion TLP:**
```
DW0: [31:24] Opcode = 0x80 (COMPLETION)
     [23:2]  Reserved
     [1:0]   Status (0=Success, 1=Error)
DW1: [31:0]  Read Data (or 0 for writes)
DW2: [31:0]  Reserved
DW3: [31:0]  Reserved
```

### 2. TLP Handshake Protocol

Each TLP interface uses valid/ready handshaking:

```verilog
// Request TLP transmission
input wire  req_tlp_valid,   // Sender: "I have a packet"
output wire req_tlp_ready,   // Receiver: "I can accept"
// Transaction occurs when BOTH are high for one cycle

// Completion TLP reception
output wire cpl_tlp_valid,   // Sender: "Completion ready"
input wire  cpl_tlp_ready,   // Receiver: "I can accept"
```

This decouples the modules and allows for realistic backpressure modeling.

### 3. Transaction Layer Latency

The endpoint model simulates realistic PCIe latency:

```verilog
reg [1:0] delay_count;

// When request received:
delay_count <= 2'd2;

// Countdown each cycle:
if (delay_count != 2'd0)
    delay_count <= delay_count - 1'b1;
else
    cpl_valid <= 1'b1;  // Send completion
```

This teaches software engineers that hardware operations are **not instantaneous**.

### 4. Endpoint Configuration Space

The simulated endpoint implements standard PCIe configuration registers:

```verilog
// Offset 0x000: Vendor ID / Device ID
case (saved_reg)
    10'h000: cpl_dw1 <= 32'h56781234;  // [31:16]=DevID, [15:0]=VendorID
    
    // Lab H2 specific: Writable scratch register
    10'h040: cpl_dw1 <= cfg_scratch;
    
    default: cpl_dw1 <= 32'hFFFFFFFF;  // Unimplemented
endcase
```

### 5. Absent Device Detection

PCIe enumeration requires detecting absent devices:

```c
// Read from non-existent device (Bus 0, Device 2, Function 0)
const uint32_t absent_bdf = M3DS_PCIE_BDF(0U, 2U, 0U);
config_read32(absent_bdf, 0x000U, &g_absent_value);

// Must return all 1's to indicate absence
if (g_absent_value != 0xFFFFFFFFU)
    goto failed;
```

The endpoint model implements this:

```verilog
if (saved_bdf != ENDPOINT_BDF)
    cpl_dw1 <= 32'hFFFFFFFF;  // Absent device response
```

## Module Descriptions

### Software Layer

#### Five-Stage Test Sequence

**Stage 1: Hardware Initialization**
**Stage 2: Configuration Read (Vendor/Device ID)**
**Stage 3: Configuration Write (Scratch Register)**
**Stage 4: Read-Back Verification**
**Stage 5: Absent Device Detection**

### Hardware Layer (RTL)

#### TLP Transmit Encoder

Converts high-level requests into TLP packets:

**Inputs:**
- `req_valid`, `req_ready` - Request handshake
- `req_type[1:0]` - Command type (1=read, 2=write)
- `req_bdf[31:0]` - Target device BDF
- `req_reg[9:0]` - Configuration register offset
- `req_wdata[31:0]` - Write data (for write commands)

**Outputs:**
- `tlp_valid`, `tlp_ready` - TLP handshake
- `tlp_dw0..3[31:0]` - 4-DWORD TLP packet

**Encoding Logic:**
```verilog
assign tlp_dw0 = {opcode[7:0], 14'b0, req_reg[9:0]};
assign tlp_dw1 = req_bdf;
assign tlp_dw2 = req_wdata;
assign tlp_dw3 = 32'h00000000;
```

#### Endpoint Simulation

Simulates a realistic PCIe endpoint device with:

**Configuration Space:**
- `0x000`: Vendor ID (0x1234) / Device ID (0x5678)
- `0x040`: Writable scratch register (Lab H2 specific)
- Other offsets: Return 0xFFFFFFFF (unimplemented)

**Transaction Processing:**
1. Accept request TLP via handshake
2. Save opcode, BDF, register, write data
3. Set `pending` flag and `delay_count = 2`
4. Count down delay (simulates PCIe link latency)
5. Generate completion TLP
6. Send completion via handshake

#### TLP Receive Decoder

Extracts completion status and data from completion TLPs:

**Inputs:**
- `tlp_valid`, `tlp_ready` - TLP handshake
- `tlp_dw0..3[31:0]` - Completion TLP packet

**Outputs:**
- `cpl_valid`, `cpl_ready` - Completion handshake
- `cpl_status[1:0]` - Status (0=success, other=error)
- `cpl_rdata[31:0]` - Read data payload

**Decoding Logic:**
```verilog
assign cpl_status = tlp_dw0[1:0];    // Extract status bits
assign cpl_rdata  = tlp_dw1[31:0];   // Extract data from DW1
```

#### TLP Pipeline Integration

Connects the TLP modules into a complete backend:

```verilog
labh2_pcie_tlp_tx u_tlp_tx (
    .req_valid  (req_valid),
    .req_ready  (req_ready),
    .req_type   (req_type),
    .req_bdf    (req_bdf),
    .req_reg    (req_reg),
    .req_wdata  (req_wdata),
    
    .tlp_valid  (req_tlp_valid),
    .tlp_ready  (req_tlp_ready),
    .tlp_dw0    (req_tlp_dw0),
    // ...
);

labh2_pcie_endpoint_model u_endpoint (
    .clk        (clk),
    .resetn     (resetn),
    
    .req_valid  (req_tlp_valid),
    .req_ready  (req_tlp_ready),
    .req_dw0    (req_tlp_dw0),
    // ...
    
    .cpl_valid  (cpl_tlp_valid),
    .cpl_ready  (cpl_tlp_ready),
    .cpl_dw0    (cpl_tlp_dw0),
    // ...
);

labh2_pcie_tlp_rx u_tlp_rx (
    .tlp_valid  (cpl_tlp_valid),
    .tlp_ready  (cpl_tlp_ready),
    .tlp_dw0    (cpl_tlp_dw0),
    // ...
    
    .cpl_valid  (cpl_valid),
    .cpl_ready  (cpl_ready),
    .cpl_status (cpl_status),
    .cpl_rdata  (cpl_rdata)
);
```
## Test Flow and Expected Results

### Successful Execution

At each debug checkpoint, GDB should show:

**After Stage 1 (Initialization):**
```
(gdb) print g_stage
$1 = 1
(gdb) print/x g_version
$2 = 0x10000
```

**After Stage 2 (Config Read):**
```
(gdb) print g_stage
$3 = 2
(gdb) print/x g_id
$4 = 0x56781234
(gdb) print/x g_vendor
$5 = 0x1234
(gdb) print/x g_device
$6 = 0x5678
(gdb) print g_result
$7 = 0    # LABH2_OK
```

**After Stage 3 (Config Write):**
```
(gdb) print g_stage
$8 = 3
(gdb) print g_result
$9 = 0    # LABH2_OK
```

**After Stage 4 (Read-Back Verification):**
```
(gdb) print g_stage
$10 = 4
(gdb) print/x g_readback
$11 = 0xa5a55a5a    # Matches written value
(gdb) print g_result
$12 = 0             # LABH2_OK
```

**After Stage 5 (Absent Device):**
```
(gdb) print g_stage
$13 = 5
(gdb) print/x g_absent_value
$14 = 0xffffffff    # Absent device indicator
(gdb) print g_result
$15 = 0             # LABH2_OK
```

**Final Success:**
```
(gdb) print g_stage
$16 = 6    # All tests passed
```
### Key Signal Groups

#### 1. TLP Request Interface
Monitor packet flow from TX to endpoint:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `req_tlp_valid` | TX has packet | Asserts when encoding complete |
| `req_tlp_ready` | Endpoint ready | Should be high (endpoint not busy) |
| `req_tlp_dw0[31:24]` | Opcode | 0x01 (read) or 0x02 (write) |
| `req_tlp_dw0[9:0]` | Register offset | 0x000, 0x040, etc. |
| `req_tlp_dw1` | BDF | 0x00000800 for endpoint |
| `req_tlp_dw2` | Write data | 0xA5A55A5A for stage 3 |

#### 2. Endpoint Internal State
Observe transaction processing:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `pending` | Transaction in progress | Set when request accepted |
| `delay_count[1:0]` | Latency counter | 2 → 1 → 0 → completion |
| `cfg_scratch[31:0]` | Scratch register | Updates to 0xA5A55A5A in stage 3 |
| `saved_opcode[7:0]` | Saved command | Stores request opcode |
| `saved_bdf[31:0]` | Saved BDF | Stores target device |

#### 3. TLP Completion Interface
Monitor response flow:

| Signal | Description | Expected Behavior |
|--------|-------------|-------------------|
| `cpl_tlp_valid` | Completion available | Asserts after delay |
| `cpl_tlp_ready` | RX decoder ready | Should be high |
| `cpl_tlp_dw0[31:24]` | Completion opcode | 0x80 (CPL) |
| `cpl_tlp_dw0[1:0]` | Status | 0 (success) |
| `cpl_tlp_dw1` | Read data | 0x56781234, 0xA5A55A5A, etc. |


## Comparison: Lab H1 vs Lab H2

| Aspect | Lab H1 | Lab H2 |
|--------|--------|--------|
| **Backend Model** | Simplified stub with direct response | Full TLP encoding/decoding pipeline |
| **Transaction Protocol** | Abstract request/completion | PCIe TLP packets (DW0-DW3) |
| **Latency Model** | Simple delay counter | Transaction-layer delay + handshakes |
| **Hardware Modules** | 1 (backend stub) | 4 (TX, RX, endpoint, backend) |
| **Software Testing** | Read-only verification | Read + write + verification |
| **Configuration Space** | Fixed Vendor/Device ID only | Multiple registers + writable scratch |
| **Absent Device** | Not implemented | Proper 0xFFFFFFFF response |
| **Verification** | Basic functional test | Unit + integration + waveform |
| **Educational Focus** | AHB-to-register interface | PCIe protocol fundamentals |
| **Realism** | Functional placeholder | Industry-realistic protocol |

## References

- **ARM DDI 0337I** - Cortex-M3 Technical Reference Manual
- **ARM IHI 0033B** - AMBA AHB-Lite Protocol Specification
- **PCI Express Base Specification** - Transaction Layer Protocol
- **Lab H1 README** - AHB PCIe Host Bridge foundation
- **CMSIS-Core** - Cortex-M register access patterns
