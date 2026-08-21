# Cortex-M3 Embedded Debug Labs for Beginners - From First Debug to ARMv7

Hands-on ARM Cortex-M3 laboratories designed for embedded software engineers to understand ARMv7-M architecture through cross compilation, low-level debugging, CMSIS, exception handling, and privilege management. 

The labs are designed to be executed on Linux using the GNU Arm Embedded Toolchain together with QEMU and GDB.

## Who This Is For

- **Embedded software engineers** transitioning to ARM Cortex-M platforms
- **BSP developers** building board support packages and drivers
- **Firmware engineers** needing deep understanding of ARMv7-M architecture
- **Students** learning bare-metal embedded programming
- **Anyone** wanting hands-on experience with low-level debugging

## Learning Objectives

This repository focuses on the practical skills required by embedded software and BSP engineers.

### Core Architecture Topics
- Cross Compilation for ARM Cortex-M3
- Cortex-M3 Startup Code and Boot Sequence
- Linker Scripts and Memory Layout
- CMSIS Core Register Access
- ARM Assembly (Thumb-2)
- Processor Modes and Privilege Management
- MSP / PSP (Dual Stack Architecture)
- EXC_RETURN Mechanism

### Exception and Interrupt Handling
- Exception Handling Fundamentals
- Hardware Exception Stack Frame
- NVIC (Nested Vectored Interrupt Controller)
- Interrupt-Driven I/O

### Peripheral and Driver Development
- Memory-Mapped I/O (MMIO)
- UART Communication (Polling, Interrupt, Ring Buffer)
- I2C Protocol and EEPROM Access
- SPI Protocol and Hardware Controllers
- Multi-Layer Driver Architecture

### Debug and Analysis Tools
- Low-level Debugging with GDB
- Binary Analysis (objdump, readelf, nm)
- QEMU Emulation
- Optimization Analysis

## Development Environment

| Component | Description |
|-----------|-------------|
| **Host** | [Ubuntu Linux](https://ubuntu.com/download) (or compatible Linux distribution) |
| **Toolchain** | [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) (arm-none-eabi-*) |
| **Emulator** | QEMU with ARM MPS2 support |
| **Debugger** | GDB / gdb-multiarch |
| **Platform** | Arm MPS2 + AN385 (Cortex-M3) |
| **Architecture** | ARMv7-M / Cortex-M3 |
| **AI Assistant** | Kiro CLI |

## Prerequisites

Check your development environment:

```bash
make doctor
```

## Repository Structure

```text
cortex-m3-embedded-debug-labs/
├── Makefile                  # Build and run all labs
├── scripts/                  # Development workflow scripts
│   ├── tmux.sh              # Launch tmux-based development environment
│   ├── lab-switch.sh        # Quick lab switching in tmux
│   ├── kiro-helper.sh       # AI-powered lab assistance
│   ├── doctor.sh            # Environment verification
│   └── README.md            # Scripts documentation
│
├── platform/                 # Platform-specific code
│   ├── baremetal/           # Minimal startup (labs 00-05)
│   │   ├── device.h         # Hardware definitions
│   │   ├── linker.ld        # Memory layout
│   │   ├── startup.s        # Minimal startup code
│   │   └── Makefile.common  # Build rules
│   │
│   ├── runtime/             # Full C runtime (labs 06+)
│   │   ├── device.h         # Hardware definitions
│   │   ├── linker.ld        # Memory layout with .data/.bss
│   │   ├── startup.s        # Full startup with initialization
│   │   ├── runtime.c        # C runtime support
│   │   ├── syscalls.c       # Newlib syscall stubs
│   │   └── Makefile.common  # Build rules
│   │
│   └── selftest/            # Platform self-test utilities
│
├── labs/                     # Hands-on laboratories
│   ├── lab00_cross_compile/              # Introduction to ARM cross-compilation
│   ├── lab01_core_registers/             # CMSIS core register access
│   ├── lab02_interrupt_control/          # NVIC and interrupt enabling
│   ├── lab03_svc_exception/              # Supervisor call and EXC_RETURN
│   ├── lab04_stack_frame/                # Hardware exception stack frame
│   ├── lab05_privilege_stack/            # Privilege modes and MSP/PSP
│   ├── lab06_startup_runtime/            # Full C runtime initialization
│   ├── lab07_optimization/               # Compiler optimization effects
│   ├── lab08_uart_register/              # Direct UART register access
│   ├── lab09_uart_polling/               # Polled UART I/O
│   ├── lab10_uart_interrupt/             # Interrupt-driven UART
│   ├── lab11_uart_ringbuffer/            # Ring buffer for UART data
│   ├── lab12_uart_driver_abstraction/    # Layered driver architecture
│   ├── lab13_i2c_transaction/            # I2C protocol fundamentals
│   ├── lab14_mps2_mmio_i2c/              # Hardware I2C with EEPROM
│   ├── lab15_spi_transaction/            # Software SPI bit-banging
│   └── lab16_hardware_spi_controller/    # Hardware SPI peripheral
│
└── tests/                    # Repository self-tests
```

## Quick Start with Tmux + Kiro CLI
For an optimized development workflow with AI assistance:

```text
# Start tmux environment with specific lab
./scripts/tmux.sh cortexm3 lab01_core_registers

# Switch to different lab (inside tmux session)
./scripts/lab-switch.sh 05

# Get AI-powered lab analysis
./scripts/kiro-helper.sh analyze-lab lab01_core_registers
```

See [scripts/README.md](scripts/README.md) and [scripts/TMUX_WORKFLOW.md](scripts/TMUX_WORKFLOW.md) for details.
