# Lab 05: Privilege and Stack Management

## Overview

This lab demonstrates the Cortex-M3's dual-stack architecture and privilege level management. It shows how to switch between privileged and unprivileged modes, transition from Main Stack Pointer (MSP) to Process Stack Pointer (PSP), and inspect the hardware exception stack frame. These concepts are fundamental to understanding RTOS implementations and secure embedded systems.

## Learning Objectives

- Understand Cortex-M3 privilege levels (Privileged vs Unprivileged)
- Learn dual stack pointer architecture (MSP vs PSP)
- Master the CONTROL register for mode switching
- Understand EXC_RETURN mechanism for stack determination
- Inspect hardware exception stack frames
- Implement proper SVC (Supervisor Call) exception handlers

## Key Concepts

### Privilege Levels

The ARMv7-M architecture supports two privilege levels:

#### Privileged Mode
- **Full access** to all processor resources
- Can access all memory regions
- Can modify CONTROL register
- Can execute all instructions
- **Default mode** at reset

#### Unprivileged Mode
- **Restricted access** to certain registers
- Cannot modify CONTROL register directly
- Cannot access System Control Space (SCS)
- Limited memory access (MPU enforced)
- Must use SVC to request privileged operations

### Dual Stack Architecture

The Cortex-M3 provides two stack pointers:

#### MSP (Main Stack Pointer)
- **Default** stack pointer at reset
- Used for **exception handlers** and **kernel code**
- Always used in Handler mode
- Remains privileged and isolated

#### PSP (Process Stack Pointer)
- Used for **application tasks** in Thread mode
- Enables **per-task stacks** in RTOS
- Can be used in privileged or unprivileged mode
- Isolates application from kernel

### CONTROL Register

Controls privilege and stack selection:

```
Bits    Name     Description
------  -------  ------------------------------------------
[31:2]  -        Reserved
[1]     SPSEL    Stack Pointer Selection
                 0 = MSP (default)
                 1 = PSP
[0]     nPRIV    Privilege Level
                 0 = Privileged (default)
                 1 = Unprivileged
```

**Important**: Changes to CONTROL require an ISB (Instruction Synchronization Barrier) instruction to take effect.

### EXC_RETURN

When an exception occurs, the Link Register (LR) contains a special **EXC_RETURN** value that encodes exception context:

```
Bit 2: Stack used during exception
       0 = MSP was active
       1 = PSP was active
```

This is critical for exception handlers to determine which stack contains the saved context.

### Hardware Exception Stack Frame

On exception entry, the Cortex-M3 **automatically** pushes 8 registers onto the active stack:

```
Higher Address
+----------------+
|     xPSR       |  [7]  Program Status Register
+----------------+
|      PC        |  [6]  Return address
+----------------+
|      LR        |  [5]  Link Register
+----------------+
|     R12        |  [4]
+----------------+
|      R3        |  [3]
+----------------+
|      R2        |  [2]
+----------------+
|      R1        |  [1]
+----------------+
|      R0        |  [0]  <- Stack Pointer
+----------------+
Lower Address
```

## Debug Session Example

```gdb
# Set breakpoint before mode switch
(gdb) break main.c:86
Breakpoint 1 at 0x...: file main.c, line 86.

(gdb) continue

# Inspect initial state (Privileged, MSP)
(gdb) print/x control_before
$1 = 0x0

(gdb) print/x msp_before
$2 = 0x20007ff0

(gdb) print/x psp_before
$3 = 0x0

# Continue to after mode switch
(gdb) break main.c:97
(gdb) continue

(gdb) print/x psp_after
$4 = 0x20007e00    # Points to process_stack

# Set breakpoint in SVC handler
(gdb) break SVC_Handler_C
(gdb) continue

# Inspect EXC_RETURN
(gdb) print/x lr
$5 = 0xfffffffd    # Bit 2 = 1 (PSP was active)

# Inspect stack frame
(gdb) print/x dump[0]
$6 = 0x11111111    # R0

(gdb) print/x dump[1]
$7 = 0x22222222    # R1

(gdb) print/x dump[2]
$8 = 0x33333333    # R2

(gdb) print/x dump[3]
$9 = 0x44444444   # R3

(gdb) print/x dump[6]
$10 = 0x...        # PC (return address after SVC)

# View complete stack frame
(gdb) x/8xw stack
```

## Expected Results

### Before Mode Switch

| Register | Value | Meaning |
|----------|-------|---------|
| control_before | `0x00000000` | Privileged, MSP active |
| msp_before | `0x20007FF0` | Main stack at top of RAM |
| psp_before | `0x00000000` | PSP not initialized |

### After Mode Switch

| Register | Value | Meaning |
|----------|-------|---------|
| control_after | `0x00000003` | Unprivileged, PSP active |
| msp_after | `0x20007FF0` | MSP unchanged |
| psp_after | `0x20007E00` | Points to process_stack[64] |

### EXC_RETURN Value

```
0xFFFFFFFD = 1111 1111 1111 1111 1111 1111 1111 1101

Bit 2 = 1: Return to Thread mode using PSP
Bit 1 = 0: No floating-point context
Bit 0 = 1: Return to Thread mode
```

### Exception Stack Frame

| Index | Register | Value | Description |
|-------|----------|-------|-------------|
| dump[0] | R0 | `0x11111111` | First argument |
| dump[1] | R1 | `0x22222222` | Second argument |
| dump[2] | R2 | `0x33333333` | Third argument |
| dump[3] | R3 | `0x44444444` | Fourth argument |
| dump[4] | R12 | `0x????????` | Scratch register |
| dump[5] | LR | `0x????????` | Return address before SVC |
| dump[6] | PC | `0x????????` | Instruction after SVC |
| dump[7] | xPSR | `0x????????` | Program status |


## Common GDB Commands

```gdb
# View special registers
info registers
info registers control
info registers msp
info registers psp

# Examine stack memory
x/16xw $psp
x/16xw $msp

# View CONTROL register bits
print/t $control    # Binary format

# Disassemble handler
disassemble SVC_Handler
disassemble SVC_Handler_C

# Step through assembly
stepi    # Step one instruction
```

## Key Takeaways

1. **Dual Privilege Model**: Privileged mode for OS, unprivileged for tasks
2. **Dual Stack Model**: MSP for kernel, PSP for applications
3. **CONTROL Register**: Single point to configure privilege and stack
4. **EXC_RETURN**: Encodes exception context for proper return
5. **Hardware Stacking**: Cortex-M3 automatically saves/restores context
6. **Naked Functions**: Required when managing stack/context manually
7. **SVC Pattern**: Foundation for RTOS system calls
8. **ISB Requirement**: CONTROL changes need synchronization

## RTOS Connection

This lab demonstrates the core mechanisms used by **every ARM Cortex-M RTOS**:

- **FreeRTOS**: Tasks run on PSP in unprivileged mode
- **Zephyr**: Uses PSP for thread stacks
- **ThreadX**: Leverages dual-stack for task isolation
- **RTX**: CMSIS-RTOS uses PSP for threads

The pattern is universal:
```
Kernel (MSP, Privileged) → Schedule Task → Switch to PSP → Run Task (Unprivileged)
```

## References

- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARMv7-M Architecture Reference Manual - Exception Model](https://developer.arm.com/documentation/ddi0403/latest/)
- [ARM Cortex-M Programming Guide](https://developer.arm.com/documentation/den0013/latest/)
- [CMSIS-Core Documentation](https://arm-software.github.io/CMSIS_5/Core/html/index.html)
