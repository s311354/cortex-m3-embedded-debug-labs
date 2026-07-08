# Cortex-M3 Embedded Debug Labs for Beginners - From First Debug to ARMv7

Hands-on ARM Cortex-M3 laboratories designed for embedded software engineers to understand ARMv7-M architecture through cross compilation, low-level debugging, CMSIS, exception handling, and privilege management. 

The labs are designed to be executed on Linux using the GNU Arm Embedded Toolchain together with QEMU and GDB.

# Learning Objectives

This repository focuses on the practical skills required by embedded software and BSP engineers.

Topics include:
- Cross Compilation
- Cortex-M3 Startup Code
- Linker Script
- CMSIS Core Register Access
- ARM Assembly (Thumb-2)
- Exception Handling
- Hardware Exception Stack Frame
- Interrupt Control
- Processor Modes
- Privilege Management
- MSP / PSP
- EXC_RETURN
- Low-level Dubugging
- GDB
- objdump
- readelf
- QEMU

# Development Environment

Host
- [Ubuntu Linux](https://ubuntu.com/download)

ToolChain
- [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)

Emulator
- QEMU

Debugger
- GDB / gdb-multiarch

Architecture
- ARMv7-M
- Cortex-M3

# Repository Structure

```text
cortex-m3-embedded-debug-labs/
├── Makefile                  # Build all labs
├── platform/
│   ├── baremetal/
│   │   ├── device.h
│   │   ├── linker.ld
│   │   ├── startup.s          # Minimal Startup
│   │   └── Makefile.common
│   │
│   └── runtime/
│       ├── linker.ld
│       ├── startup.s          # Full C Runtime Startup
│       ├── Makefile.common
│       └── runtime.c
│
├── labs/
│   ├── lab00_cross_compile/
│   ├── lab01_core_registers/
│   ├── lab02_interrupt_control/
│   ├── lab03_svc_exception/
│   ├── lab04_exception_stack/
│   ├── lab05_privilege_stack/
│   ├── lab06_runtime/
│   ├── lab07_optimization/
│   ├── lab08_uart_register/
│   ├── lab09_uart_polling/
│   ├── lab10_uart_interrupt/
│   ├── lab11_uart_ringbuffer/
└─  └── lab12_uart_driver_abstraction/
```

