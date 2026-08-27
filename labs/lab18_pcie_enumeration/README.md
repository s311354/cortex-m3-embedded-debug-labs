# Lab 18: PCIe Bus Enumeration - Multi-Layer Driver Architecture

## Overview

This lab demonstrates a **multi-layer driver architecture** for PCIe bus enumeration. While PCIe is not typical in Cortex-M3 systems, this lab teaches fundamental embedded driver design patterns applicable to any bus protocol (I2C, SPI, USB, CAN, etc.). The implementation features hardware abstraction, clean layering, simulated hardware for testing, and debuggable state tracking.

## Learning Objectives

- Design multi-layer driver architectures with clean abstractions
- Implement hardware abstraction layers (HAL) using function pointers
- Master bus enumeration and device discovery algorithms
- Parse and decode hardware configuration registers
- Use simulation layers for driver testing without physical hardware
- Design debuggable embedded code with volatile state tracking
- Handle errors with proper propagation across layers
- Validate hardware register access (alignment, bounds checking)
- Understand BDF (Bus/Device/Function) addressing schemes

## Architecture Overview

```
┌─────────────────────────────────────────┐
│  Application Layer (main.c)              │
│  - Initialization sequence               │
│  - Device discovery coordination         │
│  - Result capture and validation         │
└──────────────┬──────────────────────────┘
               │ board_pcie_init()
               │ pcie_bus_init()
               │ pcie_enumerate()
               ▼
┌─────────────────────────────────────────┐
│  Bus Management Layer (pcie_bus.c)       │
│  - Bus/Device/Function enumeration       │
│  - Configuration space parsing           │
│  - Device structure population           │
│  - BAR (Base Address Register) decoding  │
└──────────────┬──────────────────────────┘
               │ pcie_config_read32()
               │ pcie_config_write32()
               ▼
┌─────────────────────────────────────────┐
│  HAL - Host Interface (pcie_host.c)      │
│  - Configuration space access            │
│  - BDF validation (device ≤31, fn ≤7)   │
│  - Offset validation (aligned, <256)     │
│  - Function pointer dispatch             │
└──────────────┬──────────────────────────┘
               │ ops->config_read32()
               │ ops->config_write32()
               ▼
┌─────────────────────────────────────────┐
│  Platform Layer (board_pcie.c)           │
│  - Board-specific initialization         │
│  - Host controller binding               │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│  Simulation Layer (pcie_sim_host.c)      │
│  - Simulated PCIe configuration space    │
│  - 4 virtual devices (MMIO arrays)       │
│  - Function lookup by BDF                │
└─────────────────────────────────────────┘
```

## Key Concepts

### 1. PCIe Configuration Space

PCIe devices expose a 256-byte (PCIe: 4KB) configuration space containing device information:

```
Offset  Register              Description
------  --------------------  ----------------------------------
0x00    VENDOR_DEVICE         Vendor ID | Device ID
0x04    COMMAND_STATUS        Command and Status registers
0x08    CLASS_REVISION        Class code, Subclass, Prog IF, Revision
0x0C    HEADER               Cache line, Latency, Header type, BIST
0x10    BAR0                 Base Address Register 0
0x14    BAR1                 Base Address Register 1
...     ...                  ...
0x3C    INTERRUPT            IRQ Line | IRQ Pin
```

**Key Fields:**
- **Vendor ID**: 0xFFFF indicates no device present
- **Class Code**: Device type (0x02=Network, 0x01=Mass Storage, etc.)
- **Header Type**: 0x00=Normal, 0x01=Bridge, 0x80=Multifunction bit
- **BARs**: Base addresses for MMIO or I/O regions

### 2. BDF Addressing (Bus/Device/Function)

PCIe uses hierarchical addressing:
- **Bus**: Up to 256 buses (0-255)
- **Device**: Up to 32 devices per bus (0-31)
- **Function**: Up to 8 functions per device (0-7)

```c
struct pcie_bdf {
    uint8_t bus;
    uint8_t device;    // Max 31
    uint8_t function;  // Max 7
};
```

### 3. BAR Decoding (Base Address Registers)

BARs specify device memory or I/O regions:

```
Bit 0: Region type (0=Memory, 1=I/O)

Memory BAR:
  Bits [1:2]: Type (00=32-bit, 10=64-bit)
  Bit 3:      Prefetchable
  Bits [31:4]: Base address (16-byte aligned)

I/O BAR:
  Bits [31:2]: Base address (4-byte aligned)
```

```c
struct pcie_bar {
    uint32_t raw;
    uint32_t address;
    uint8_t is_io;
    uint8_t is_64bit;
    uint8_t prefetchable;
};
```

### 4. Enumeration Algorithm

The bus scan follows a hierarchical pattern:

```
pcie_enumerate()
  └─ pcie_scan_bus(bus=0)
       └─ for device in 0..31:
            └─ pcie_scan_device(bus, device)
                 ├─ Read vendor ID from function 0
                 ├─ If vendor != 0xFFFF: scan function 0
                 ├─ Check multifunction bit in header
                 └─ If multifunction: scan functions 1-7
                      └─ pcie_scan_function(bus, device, fn)
                           ├─ Read vendor ID
                           ├─ If vendor != 0xFFFF:
                           └─ pcie_read_function() - populate device struct
```

### 5. Hardware Abstraction Pattern

The HAL uses function pointers for platform independence:

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

struct pcie_host {
    const struct pcie_host_ops *ops;
    void *priv;  // Platform-specific context
};
```

This allows swapping implementations (simulation, real hardware, different controllers) without changing upper layers.

### 6. Simulated Hardware for Testing

The simulation layer creates 4 virtual PCIe devices:

```c
Device 0: Bus 0, Device 1, Function 0
  - Vendor: 0x1234, Device: 0x5678
  - Class: 0x02 (Network), Subclass: 0x00 (Ethernet)
  - BAR0: 0x50000000
  - IRQ: Line=5, Pin=1

Device 1: Bus 0, Device 3, Function 0
  - Vendor: 0x0ABC, Device: 0x1000
  - Class: 0x01 (Mass Storage), Subclass: 0x06 (SATA)
  - BAR0: 0x51000000
  - IRQ: Line=7, Pin=1

Device 2: Bus 0, Device 5, Function 0
  - Vendor: 0xCAFE, Device: 0x0001
  - Class: 0x04 (Multimedia), Subclass: 0x01 (Audio)
  - BAR0: 0x52000000
  - IRQ: Line=9, Pin=1
  - Header Type: 0x80 (Multifunction)

Device 3: Bus 0, Device 5, Function 1
  - Vendor: 0xCAFE, Device: 0x0002
  - Class: 0x03 (Display), Subclass: 0x00 (VGA)
  - BAR0: 0x53000000
  - IRQ: Line=10, Pin=1
```

Note: Device 5 is multifunction (bit 7 set in header type), so functions 0 and 1 both exist.

## Debug Strategy

### Global State Tracking

The lab uses volatile globals for GDB inspection:

```c
volatile uint32_t g_stage;          // Execution stage (1-4, 0xFFFFFFFF=error)

volatile int g_board_result;        // board_pcie_init() result
volatile int g_bus_result;          // pcie_bus_init() result
volatile int g_enum_result;         // pcie_enumerate() result

volatile uint32_t g_device_count;   // Number of devices found

volatile uint16_t g_first_vendor;   // First device vendor ID
volatile uint16_t g_first_device;   // First device device ID
volatile uint32_t g_first_bar0;     // First device BAR0
volatile uint8_t g_first_irq_line;  // First device IRQ line
volatile uint8_t g_first_irq_pin;   // First device IRQ pin
```

### Execution Stages

```
g_stage = 1: After board initialization
g_stage = 2: After bus initialization
g_stage = 3: After enumeration complete
g_stage = 4: After device data capture (success)
g_stage = 0xFFFFFFFF: Error occurred
```

## GDB Debug Session

### Initial Setup

```gdb
# Set breakpoints at key points
(gdb) break main
(gdb) break lab18_debug_checkpoint
(gdb) break pcie_enumerate

# Start execution
(gdb) run
```

### Stage 1: Board Initialization

```gdb
# At first checkpoint (stage 1)
(gdb) continue

# Check initialization result
(gdb) print g_stage
$1 = 1

(gdb) print g_board_result
$2 = 0    # PCIE_OK

# Examine board host structure
(gdb) print g_board_pcie_host
$3 = {
  ops = 0x...,
  priv = 0x...
}

(gdb) print g_board_pcie_host.ops->config_read32
$4 = {int (struct pcie_host *, struct pcie_bdf, uint16_t, uint32_t *)} 0x... <sim_config_read32>
```

### Stage 2: Bus Initialization

```gdb
# Continue to stage 2
(gdb) continue

(gdb) print g_stage
$5 = 2

(gdb) print g_bus_result
$6 = 0    # PCIE_OK

# Examine bus structure
(gdb) print g_pcie_bus
$7 = {
  host = 0x...,
  devices = {...},
  device_count = 0
}
```

### Stage 3: Enumeration

```gdb
# Step into enumeration
(gdb) break pcie_scan_function
(gdb) continue

# At first device discovery
(gdb) print bdf
$8 = {bus = 0, device = 1, function = 0}

(gdb) print vendor
$9 = 0x1234

# Continue through all devices
(gdb) continue  # Device 1
(gdb) continue  # Device 3
(gdb) continue  # Device 5 Function 0
(gdb) continue  # Device 5 Function 1

# After enumeration
(gdb) delete breakpoints  # Clear function breakpoint
(gdb) break lab18_debug_checkpoint
(gdb) continue

# Check results
(gdb) print g_stage
$10 = 3

(gdb) print g_enum_result
$11 = 0    # PCIE_OK

(gdb) print g_device_count
$12 = 4    # Found all 4 devices
```

### Stage 4: Device Data

```gdb
# Final checkpoint
(gdb) continue

(gdb) print g_stage
$13 = 4

# Examine first device (Device 1)
(gdb) print/x g_first_vendor
$14 = 0x1234

(gdb) print/x g_first_device
$15 = 0x5678

(gdb) print/x g_first_bar0
$16 = 0x50000000

(gdb) print g_first_irq_line
$17 = 5

(gdb) print g_first_irq_pin
$18 = 1
```

### Examine All Devices

```gdb
# Iterate through all discovered devices
(gdb) print g_pcie_bus.device_count
$19 = 4

(gdb) print g_pcie_bus.devices[0]
$20 = {
  bdf = {bus = 0, device = 1, function = 0},
  vendor_id = 0x1234,
  device_id = 0x5678,
  class_code = 0x02,    # Network
  subclass = 0x00,      # Ethernet
  bars = {{
    raw = 0x50000000,
    address = 0x50000000,
    is_io = 0,
    is_64bit = 0,
    prefetchable = 0
  }, ...},
  irq_line = 5,
  irq_pin = 1
}

(gdb) print g_pcie_bus.devices[1]
$21 = {
  bdf = {bus = 0, device = 3, function = 0},
  vendor_id = 0x0abc,
  device_id = 0x1000,
  class_code = 0x01,    # Mass Storage
  subclass = 0x06,      # SATA
  ...
}

(gdb) print g_pcie_bus.devices[2]
$22 = {
  bdf = {bus = 0, device = 5, function = 0},
  vendor_id = 0xcafe,
  device_id = 0x0001,
  class_code = 0x04,    # Multimedia
  subclass = 0x01,      # Audio
  header_type = 0x80,   # Multifunction!
  ...
}

(gdb) print g_pcie_bus.devices[3]
$23 = {
  bdf = {bus = 0, device = 5, function = 1},
  vendor_id = 0xcafe,
  device_id = 0x0002,
  class_code = 0x03,    # Display
  subclass = 0x00,      # VGA
  ...
}
```

### Trace Configuration Space Access

```gdb
# Set breakpoint in simulation layer
(gdb) break sim_config_read32

# Continue to see all configuration reads
(gdb) commands
Type commands for breakpoint(s) 1, one per line.
End with a line saying just "end".
>silent
>printf "Read: Bus=%d Dev=%d Fn=%d Offset=0x%02x Value=0x%08x\n", \
    bdf.bus, bdf.device, bdf.function, offset, *value
>continue
>end

(gdb) continue
```

This will show the complete sequence of configuration space reads during enumeration.

### Examine BAR Decoding

```gdb
# Look at BAR decoding logic
(gdb) print/x g_pcie_bus.devices[0].bars[0]
$24 = {
  raw = 0x50000000,
  address = 0x50000000,    # 16-byte aligned
  is_io = 0x0,             # Memory BAR
  is_64bit = 0x0,          # 32-bit BAR
  prefetchable = 0x0       # Non-prefetchable
}

# Decode the raw value manually
(gdb) print/t 0x50000000
$25 = 1010000000000000000000000000000
#     ^^^^^^^^^^^^^^^^^^^^^^^^^^^^ address [31:4]
#                                 ^ prefetchable [3]
#                                ^^ type [2:1]
#                                 ^ memory/io [0]
```

## Design Patterns Demonstrated

### 1. Hardware Abstraction Layer

```c
// Generic interface
int pcie_config_read32(struct pcie_host *host, ...);

// Platform-specific implementation via function pointers
host->ops->config_read32(host, ...);
```

**Benefits:**
- Platform independence
- Testability (simulation vs real hardware)
- Code reuse across different controllers

### 2. Opaque Context (Private Data)

```c
struct pcie_host {
    const struct pcie_host_ops *ops;
    void *priv;  // Platform can store anything here
};
```

**Benefits:**
- Encapsulation
- No coupling between layers
- Platform-specific state without polluting generic code

### 3. Error Code Propagation

```c
result = pcie_validate_bdf(bdf);
if (result != PCIE_OK)
    return result;

result = host->ops->config_read32(host, bdf, offset, value);
if (result != PCIE_OK)
    return result;
```

**Benefits:**
- Early exit on errors
- Clear error paths
- Traceable error sources

### 4. Separation of Concerns

Each layer has a single responsibility:
- **pcie_host.c**: Validation and dispatch
- **pcie_bus.c**: Bus protocol and device discovery
- **board_pcie.c**: Platform binding
- **pcie_sim_host.c**: Simulation implementation

**Benefits:**
- Maintainability
- Testability
- Clear interfaces

## Key Takeaways

1. **Layered architecture** enables code reuse and testing
2. **Function pointers** provide platform independence
3. **Validation at boundaries** catches errors early
4. **Volatile globals** aid debugging without overhead
5. **Simulation layers** enable driver development without hardware
6. **Bus enumeration** follows systematic discovery patterns
7. **Error propagation** ensures reliability
8. **Configuration registers** require careful parsing and decoding

## References

- [PCI Express Base Specification](https://pcisig.com/specifications)
- [PCI Configuration Space](https://wiki.osdev.org/PCI)
