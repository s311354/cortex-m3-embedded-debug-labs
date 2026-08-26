# Lab 17: PCIe Host Configuration Space Access

## Overview

This lab demonstrates **driver abstraction architecture** using PCIe configuration space access as the example domain. Unlike previous labs that focus on Cortex-M3 hardware features, this lab teaches **software design patterns** for building multi-layer peripheral drivers with hardware abstraction, using a simulated PCIe endpoint as the teaching vehicle.

## Learning Objectives

- Understand multi-layer driver architecture (API → Backend → Hardware)
- Implement function pointer-based operations structure pattern
- Design platform-agnostic device access APIs
- Learn PCIe configuration space structure and standard registers
- Apply input validation and error handling in driver code
- Build software simulators for testing embedded drivers
- Debug multi-layer software with breakpoints and global state inspection

## Architecture

The lab implements a three-layer architecture common in production embedded systems:

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - Configuration space enumeration       │
│  - Device identification                 │
│  - Register parsing                      │
└──────────────┬──────────────────────────┘
               │ pcie_config_read32()
               │ pcie_config_write32()
               ▼
┌─────────────────────────────────────────┐
│  Generic PCIe API (pcie_host.c)         │
│  - BDF validation                        │
│  - Offset validation                     │
│  - Dispatch to backend ops               │
└──────────────┬──────────────────────────┘
               │ ops->config_read32()
               │ ops->config_write32()
               ▼
┌─────────────────────────────────────────┐
│  Simulated Backend (pcie_sim_host.c)    │
│  - Software-simulated PCIe endpoint      │
│  - 256-byte configuration space          │
│  - Device matching logic                 │
└──────────────┬──────────────────────────┘
               │ Direct memory access
               ▼
┌─────────────────────────────────────────┐
│  Configuration Space (RAM array)         │
│  - Vendor/Device ID: 0x1234/0x5678      │
│  - Class Code: 0x02 (Network)           │
│  - BAR0: 0x50000000                      │
└─────────────────────────────────────────┘
```

### Layer Responsibilities

**Application Layer** (`main.c`)
- Enumerate PCIe devices by BDF (Bus/Device/Function)
- Read standard configuration registers
- Parse and extract device information
- Demonstrate typical PCIe discovery workflow

**Generic API Layer** (`pcie_host.c`)
- Platform-agnostic PCIe access functions
- Input validation (BDF ranges, offset alignment)
- Error handling
- Dispatch to backend implementation

**Simulated Backend** (`pcie_sim_host.c`)
- Software simulation of PCIe endpoint
- Implements `pcie_host_ops` interface
- Device matching and configuration space storage

**Board Integration** (`board_pcie.c`)
- Connects generic API to specific backend
- Initialization and setup
- Could be swapped for real hardware backend

## Key Data Structures

### Operations Structure Pattern

```c
struct pcie_host_ops {
    int (*config_read32)(struct pcie_host *host, 
                         struct pcie_bdf bdf, 
                         uint16_t offset, 
                         uint32_t *value);
    int (*config_write32)(struct pcie_host *host, 
                          struct pcie_bdf bdf, 
                          uint16_t offset, 
                          uint32_t value);
};
```

This is the same pattern used in:
- Linux kernel (`struct file_operations`, `struct pci_ops`)
- CMSIS driver specifications
- Embedded driver frameworks

### Device Addressing

```c
struct pcie_bdf {
    uint8_t bus;        // 0-255
    uint8_t device;     // 0-31 (hardware limit)
    uint8_t function;   // 0-7 (hardware limit)
};
```

PCIe uses Bus/Device/Function (BDF) addressing:
- **Bus**: PCIe hierarchy level (root → switches → endpoints)
- **Device**: Physical device on a bus (max 32 per bus)
- **Function**: Logical function within device (max 8 per device)

### Host Controller Structure

```c
struct pcie_host {
    const struct pcie_host_ops *ops;  // Function pointer table
    void *priv;                       // Backend-specific data
};
```

Enables polymorphism in C: same API, different implementations.

## PCIe Configuration Space

Each PCIe device has a 256-byte Type 0 configuration header (4096 bytes in PCIe extended space):

```
Offset  Name                    Lab17 Simulated Value
------  ----------------------  -----------------------
0x00    Vendor ID               0x1234
0x02    Device ID               0x5678
0x04    Command                 0x0000
0x06    Status                  0x0000
0x08    Revision ID             0x01
0x09    Programming Interface   0x00
0x0A    Sub-Class               0x00 (Ethernet)
0x0B    Class Code              0x02 (Network Controller)
0x0C    Cache Line Size         0x00
0x0D    Latency Timer           0x00
0x0E    Header Type             0x00 (Type 0, single function)
0x0F    BIST                    0x00
0x10    BAR0                    0x50000000
0x14    BAR1                    0x00000000
0x18    BAR2                    0x00000000
0x1C    BAR3                    0x00000000
0x20    BAR4                    0x00000000
0x24    BAR5                    0x00000000
```

## Validation Logic

### BDF Validation

```c
// Device: 5 bits → 0-31
if (bdf.device > 31U)
    return PCIE_ERR_BDF;

// Function: 3 bits → 0-7
if (bdf.function > 7U)
    return PCIE_ERR_BDF;
```

## Expected Behavior

The program walks through a staged enumeration sequence:

1. **Stage 1**: Initialize PCIe host controller
2. **Stage 2**: Read Vendor/Device ID from endpoint at BDF 0:1.0
   - Vendor ID: `0x1234`
   - Device ID: `0x5678`
3. **Stage 3**: Read Class Code and device metadata
   - Class Code: `0x02` (Network Controller)
   - Subclass: `0x00` (Ethernet Controller)
   - Prog IF: `0x00`
   - Revision: `0x01`
   - Header Type: `0x00` (Type 0, single function)
4. **Stage 4**: Read BAR0 (Base Address Register 0)
   - BAR0: `0x50000000` (memory-mapped I/O base address)

Each stage is tracked in `g_stage` and uses `lab17_debug_checkpoint()` for GDB breakpoints.

## Debugging Exercises

### 1. Trace Initialization Sequence

```gdb
(gdb) break main
(gdb) run
(gdb) print g_stage
$1 = 1

# Step through initialization
(gdb) next
(gdb) print g_init_result
$2 = 0  # PCIE_OK

# Examine initialized endpoint
(gdb) print g_sim_controller.endpoint
$3 = {
  bdf = {bus = 0, device = 1, function = 0},
  config = {0x56781234, 0x0, 0x2000001, 0x0, 0x50000000, ...}
}
```

### 2. Watch Configuration Space Reads

```gdb
# Break at Stage 2 (Vendor/Device ID read)
(gdb) break lab17_debug_checkpoint
(gdb) continue
Breakpoint 1, lab17_debug_checkpoint () at main.c:18

(gdb) print g_stage
$4 = 2

(gdb) print/x g_id_reg
$5 = 0x56781234

(gdb) print/x g_vendor_id
$6 = 0x1234

(gdb) print/x g_device_id
$7 = 0x5678
```

### 3. Inspect Operations Structure

```gdb
(gdb) print g_board_pcie_host
$8 = {
  ops = 0x<address> <g_pcie_sim_host_ops>,
  priv = 0x<address> <g_sim_controller>
}

# Examine function pointers
(gdb) print *g_board_pcie_host.ops
$9 = {
  config_read32 = 0x<address> <sim_config_read32>,
  config_write32 = 0x<address> <sim_config_write32>
}
```

### 4. Trace Function Dispatch

```gdb
# Break at generic API call
(gdb) break pcie_config_read32

# Continue to first read
(gdb) continue
Breakpoint 2, pcie_config_read32 (host=0x<addr>, bdf=..., offset=0, 
    value=0x<addr>) at pcie_host.c:32

# Step into backend implementation
(gdb) step
sim_config_read32 (host=0x<addr>, bdf=..., offset=0, 
    value=0x<addr>) at pcie_sim_host.c:22

# Examine configuration space array
(gdb) print/x controller->endpoint.config[0]
$10 = 0x56781234
```

### 5. Analyze Register Bit Extraction

```gdb
# Break at Stage 3 after class code read
(gdb) break main.c:60
(gdb) continue

(gdb) print/x g_class_revision
$11 = 0x2000001

(gdb) print g_revision
$12 = 1

(gdb) print/x g_prog_if
$13 = 0x0

(gdb) print/x g_subclass
$14 = 0x0

(gdb) print/x g_class_code
$15 = 0x2  # Network Controller
```

### 6. Verify BDF Matching

```gdb
# The simulated endpoint only responds to BDF 0:1.0
(gdb) break bdf_equal
(gdb) continue

(gdb) print lhs
$16 = {bus = 0, device = 1, function = 0}

(gdb) print rhs
$17 = {bus = 0, device = 1, function = 0}

(gdb) print lhs.bus == rhs.bus && lhs.device == rhs.device && lhs.function == rhs.function
$18 = 1  # True - device found
```
## Connection to Embedded Systems

PCIe is common in:
- **High-performance embedded**: Network cards, NVMe storage, GPUs
- **Embedded Linux systems**: x86 and ARM servers, industrial PCs
- **FPGA platforms**: Xilinx, Intel FPGA with PCIe IP cores
- **System-on-Chip (SoC)**: Modern ARM SoCs with PCIe root complexes

Understanding PCIe configuration space is essential for:
- Device driver development
- Board bring-up and BIOS/UEFI development
- System-level debugging
- Performance optimization

## Key Takeaways

✅ Multi-layer architecture enables abstraction without sacrificing performance  
✅ Function pointers provide polymorphism in C  
✅ Input validation at API boundaries prevents error propagation  
✅ Software simulation is powerful for driver development and testing  
✅ Debug checkpoints and volatile globals aid embedded debugging  
✅ PCIe configuration space follows standardized register layout  
✅ BDF addressing enables hierarchical device organization  
✅ Operations structure pattern is industry-standard for drivers  
✅ Professional code separates generic logic from platform-specific implementation  

## References

- [PCI Local Bus Specification](https://pcisig.com/specifications)
- [PCIe Base Specification](https://pcisig.com/specifications)
