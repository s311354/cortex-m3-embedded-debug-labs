# ARM Embedded Debugging Labs for Beginners - From First Debug Session to ARMv7-M and ARMv8-A

Hands-on embedded systems labs to explore ARM architectures, low-level programming, exception handling, device drivers, and hardware–software integration through practical implementation and debugging.

Gain practical experience with bare-metal firmware, CMSIS, ACLE, and Zephyr RTOS using the Arm GNU Toolchain, QEMU, and GDB.

## Who This Is For

- **Embedded software engineers** transitioning to ARM Cortex-M platforms
- **BSP developers** building board support packages and drivers
- **Firmware engineers** needing deep understanding of ARMv7-M and ARMv8-A architecture
- **Students** learning bare-metal embedded programming
- **Anyone** wanting hands-on experience with low-level debugging

## Learning Objectives

This repository focuses on the practical skills required by embedded software and BSP engineers.

### Core Architecture Topics
- Cross Compilation for ARM Cortex-M3 and AArch64
- Startup Code and Boot Sequence
- Linker Scripts and Memory Layout
- CMSIS and ACLE Register Access
- ARM Assembly (Thumb-2 and AArch64)
- Processor Modes, Exception Levels, and Privilege Management
- MSP / PSP and AArch64 Stack Pointer Selection
- EXC_RETURN and ERET Mechanisms

### Exception and Interrupt Handling
- Exception Handling Fundamentals
- Hardware and Software Exception Stack Frames
- AArch64 Exception Vector Tables and System Registers
- NVIC (Nested Vectored Interrupt Controller)
- Interrupt Masking and Control
- Interrupt-Driven I/O

### Peripheral and Driver Development
- Memory-Mapped I/O (MMIO)
- UART Communication (Polling, Interrupt, Ring Buffer)
- I2C Protocol and EEPROM Access
- SPI Protocol and Hardware Controllers
- Multi-Layer Driver Architecture
- PCIe Host Configuration, Enumeration, and BAR Resource Management

### Zephyr RTOS and Driver Development
- Zephyr Device Driver APIs
- Asynchronous UART Communication
- I2C and SPI Device Transactions
- Flash and EEPROM Event Logging
- Watchdog Heartbeat Management
- DMA Transfer Queues

### Debug and Analysis Tools
- Low-level Debugging with GDB
- Binary Analysis (objdump, readelf, nm)
- QEMU Emulation
- Compiler Optimization Analysis
- RTL Simulation and Verification

## Development Environment

| Component | Description |
|-----------|-------------|
| **Host** | [Ubuntu Linux](https://ubuntu.com/download) (or compatible Linux distribution) |
| **Toolchain** | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (arm-none-eabi-*) |
| **Emulator** | QEMU (qemu-system-arm / qemu-system-aarch64) |
| **Debugger** | GDB / gdb-multiarch |
| **Platform** | Arm MPS2 AN385 / QEMU virt |
| **Architecture** | ARMv7-M (Cortex-M3) / ARMv8-A (Cortex-A53) |
| **RTOS** | [Zephyr RTOS](https://www.zephyrproject.org/) |
| **Build Tools** | GNU Make / CMake / West |
| **AI Assistant** | Kiro CLI |

## Prerequisites

Check your development environment:

```bash
make doctor
```

## Repository Structure

```text
cortex-m3-embedded-debug-labs/
├── Makefile                  # Build, run, and debug labs
├── scripts/                  # Development workflow scripts
│   ├── tmux.sh               # Launch tmux-based development environment
│   ├── lab-switch.sh         # Quick lab switching in tmux
│   ├── kiro-helper.sh        # AI-powered lab assistance
│   ├── doctor.sh             # Environment verification
│   └── README.md             # Scripts documentation
│
├── platform/                 # Platform-specific code
│   ├── baremetal/            # Minimal Cortex-M3 startup
│   │   ├── device.h          # Hardware definitions
│   │   ├── linker.ld         # Memory layout
│   │   ├── startup.s         # Minimal startup code
│   │   └── Makefile.common   # Build rules
│   │
│   ├── runtime/              # Full Cortex-M3 C runtime
│   │   ├── device.h          # Hardware definitions
│   │   ├── linker.ld         # Memory layout with .data/.bss
│   │   ├── startup.s         # Full startup with initialization
│   │   ├── runtime.c         # C runtime support
│   │   ├── syscalls.c        # Newlib syscall stubs
│   │   └── Makefile.common   # Build rules
│   │
│   ├── selftest/             # Platform self-test utilities
│   ├── pcie/                 # Shared PCIe components
│   ├── zephyr/               # Zephyr build support
│   └── aarch64/              # ARMv8-A platform support
│       ├── linker.ld         # AArch64 memory layout
│       ├── startup.S         # Startup and exception vectors
│       └── Makefile.common   # AArch64 build rules
│
├── labs/                     # Hands-on laboratories
│   │
│   │   # ARMv7-M Architecture
│   ├── lab00_cross_compile/
│   ├── lab01_core_registers/
│   ├── lab02_interrupt_control/
│   ├── lab03_svc_exception/
│   ├── lab04_stack_frame/
│   ├── lab05_privilege_stack/
│   ├── lab06_startup_runtime/
│   ├── lab07_optimization/
│   │
│   │   # Peripheral and Device Drivers
│   ├── lab08_uart_register/
│   ├── lab09_uart_polling/
│   ├── lab10_uart_interrupt/
│   ├── lab11_uart_ringbuffer/
│   ├── lab12_uart_driver_abstraction/
│   ├── lab13_i2c_transaction/
│   ├── lab14_mps2_mmio_i2c/
│   ├── lab15_spi_transaction/
│   ├── lab16_hardware_spi_controller/
│   │
│   │   # PCIe Host and Resource Management
│   ├── lab17_pcie_host_config/
│   ├── lab18_pcie_enumeration/
│   ├── lab19_pcie_bar_resource_zephyr/
│   │
│   │   # Zephyr RTOS
│   ├── lab20_uart_async_rx_manager_zephyr/
│   ├── lab21_i2c_register_update_zephyr/
│   ├── lab22_spi_command_read_zephyr/
│   ├── lab23_flash_boot_event_logger_zephyr/
│   ├── lab24_eeprom_fault_logger_zephyr/
│   ├── lab25_watchdog_heartbeat_zephyr/
│   ├── lab26_dma_transfer_queue_zephyr/
│   │
│   │   # Hardware–Software Integration
│   ├── labh1_ahb_pcie_host_bridge/
│   ├── labh2_pcie_transaction_model/
│   ├── labh3_fpga_pcie_root_port/
│   │
│   │   # ARMv8-A / AArch64 Architecture
│   ├── labv8a00_aarch64_cross_compile/
│   ├── labv8a01_aarch64_core_registers/
│   ├── labv8a02_aarch64_interrupt_control/
│   ├── labv8a03_aarch64_svc_exception/
│   └── labv8a04_aarch64_stack_frame/
│
└── tests/                    # Repository self-tests
```

## Quick Start with Tmux + Kiro CLI
For an optimized development workflow with AI assistance:

```text
# Start tmux environment with a specific lab
./scripts/tmux.sh cortexm3 lab01_core_registers

# Switch to another lab (inside tmux session)
./scripts/lab-switch.sh 05

# Get AI-powered lab analysis
./scripts/kiro-helper.sh analyze-lab lab01_core_registers
```
