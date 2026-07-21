# Lab 02: Interrupt Control

## Overview

This lab demonstrates interrupt masking and critical section implementation on the Cortex-M3. You'll learn how to temporarily disable interrupts to protect shared resources and implement the save-restore pattern essential for nested critical sections in real-time systems.

## Learning Objectives

- Understand the PRIMASK register and global interrupt control
- Implement critical sections using interrupt masking
- Learn the save-restore pattern for nested protection
- Use CMSIS interrupt control functions
- Debug interrupt state changes with GDB
- Apply atomic operation patterns in embedded systems

## Key Concepts

### PRIMASK Register

The PRIMASK is a 1-bit register that provides a global interrupt enable/disable mechanism:

- **PRIMASK = 0**: All configurable interrupts are **enabled**
- **PRIMASK = 1**: All configurable interrupts are **disabled**

**Important**: PRIMASK only masks configurable exceptions. NMI (Non-Maskable Interrupt) and HardFault cannot be masked and will still execute.

### CMSIS Interrupt Control Functions

```c
uint32_t __get_PRIMASK(void)    // Read current PRIMASK value
void __disable_irq(void)        // Set PRIMASK = 1 (disable interrupts)
void __enable_irq(void)         // Set PRIMASK = 0 (enable interrupts)
```

These functions compile to single ARM assembly instructions:
- `__disable_irq()` → `CPSID i` (Change Processor State, Disable Interrupts)
- `__enable_irq()` → `CPSIE i` (Change Processor State, Enable Interrupts)
- `__get_PRIMASK()` → `MRS r0, PRIMASK` (Move from special Register to general-purpose register)

### Critical Section Pattern

A **critical section** is a block of code that must execute atomically without interruption. This is essential when:
- Accessing shared variables between ISR and main code
- Performing read-modify-write operations on hardware registers
- Protecting data structures from concurrent modification

## Debug Session Example

```gdb
# Set breakpoint at main
(gdb) break main
Breakpoint 1 at 0x...: file main.c, line 14.

# Run to breakpoint
(gdb) continue

# Step to after initial read
(gdb) next
(gdb) next

# Check initial PRIMASK state
(gdb) print/x g_before
$1 = 0x0                        # Interrupts enabled

# Step past __disable_irq()
(gdb) next

# Verify interrupts are disabled
(gdb) print/x g_after_disable
$2 = 0x1                        # Interrupts disabled

# Step through critical section and restore
(gdb) next
(gdb) next
(gdb) next

# Check final state
(gdb) print/x g_after_restore
$3 = 0x0                        # Interrupts re-enabled

# View PRIMASK directly from register
(gdb) info registers
```

### Examining PRIMASK in Real-Time

```gdb
# Set watchpoint on PRIMASK state variable
(gdb) watch g_after_disable

# Display PRIMASK automatically at each stop
(gdb) display/x g_after_disable

# Single-step through interrupt control
(gdb) stepi                     # Step individual assembly instructions
```

## Expected Results

| Point in Execution | Variable | Value | State |
|-------------------|----------|-------|-------|
| Before disable | `g_before` | `0x00000000` | Interrupts enabled |
| After disable | `g_after_disable` | `0x00000001` | Interrupts disabled |
| After restore | `g_after_restore` | `0x00000000` | Interrupts enabled again |

## Understanding the Results

### g_before = 0x0
At startup, Cortex-M3 has interrupts **enabled** by default. PRIMASK is cleared after reset.

### g_after_disable = 0x1
The `__disable_irq()` function sets PRIMASK to 1, **blocking** all configurable interrupts. During this time:
- Timer interrupts won't fire
- External GPIO interrupts are masked
- UART interrupts are blocked
- SysTick is disabled
- **But**: NMI and HardFault can still execute

### g_after_restore = 0x0
Since `old` was 0 (interrupts enabled), the condition `if (!old)` evaluates to true, and interrupts are **re-enabled**.

## Advanced Topics

### BASEPRI vs PRIMASK

Cortex-M3 also has **BASEPRI** for priority-based masking:
- PRIMASK: All-or-nothing (masks everything)
- BASEPRI: Masks interrupts below a priority threshold
- More selective, better for fine-grained control

This will be explored in later labs.

### FAULTMASK

For even stricter masking:
- FAULTMASK = 1 disables **all** exceptions except NMI
- Even HardFault is blocked
- Used rarely, mainly for critical fault handlers

## Key Takeaways

1. **PRIMASK** provides simple, fast global interrupt control
2. **Save-restore pattern** enables proper nesting of critical sections
3. **Keep critical sections short** to maintain system responsiveness
4. **Use volatile** for variables inspected by debugger
5. **CMSIS functions** abstract architecture-specific assembly
6. **Conditional restore** preserves caller's interrupt state

## References

- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARMv7-M Architecture Reference Manual - Section B1.4.4 (PRIMASK)](https://developer.arm.com/documentation/ddi0403/latest/)
- [CMSIS-Core Documentation](https://arm-software.github.io/CMSIS_5/Core/html/group__Core__Register__gr.html)
- [ARM: Cortex-M Programming Guide to Memory Barrier Instructions](https://developer.arm.com/documentation/dai0321/latest/)
