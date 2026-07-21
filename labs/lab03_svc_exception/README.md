# Lab 03: SVC Exception

## Overview

This lab demonstrates the Supervisor Call (SVC) exception in the Cortex-M3 architecture. You'll learn how software-triggered exceptions work, understand the EXC_RETURN mechanism, and practice debugging exception handlers.

## Learning Objectives

- Understand the SVC (Supervisor Call) exception mechanism
- Learn about EXC_RETURN magic values
- Practice implementing naked exception handlers
- Debug exception entry and exit sequences
- Examine CPU mode transitions (Thread ↔ Handler mode)
- Understand automatic context saving/restoration

## Key Concepts

### Supervisor Call (SVC)

The `svc` instruction is a software interrupt used to request privileged operations or system services. In embedded RTOS implementations, SVC is commonly used for:
- System calls
- Context switching
- Requesting kernel services from user code

### EXC_RETURN

When entering an exception handler, the Link Register (LR) is loaded with a special value called **EXC_RETURN**. This magic value (0xFFFFFFxx) tells the processor how to return from the exception:

```
Bit Pattern: 0xFFFFFFF[1|9|D]

0xFFFFFFF1: Return to Handler mode, use MSP
0xFFFFFFF9: Return to Thread mode, use MSP
0xFFFFFFFD: Return to Thread mode, use PSP
```

The EXC_RETURN value encodes:
- Which mode to return to (Thread or Handler)
- Which stack pointer to use (MSP or PSP)
- Whether FPU context was stacked (ARMv7E-M only)

### How It Works

1. **Trigger Exception**: `svc #0` instruction triggers SVC exception
2. **Automatic Context Save**: Processor automatically pushes R0-R3, R12, LR, PC, xPSR to stack
3. **Load EXC_RETURN**: LR is loaded with EXC_RETURN value (0xFFFFFFF9 for MSP Thread mode)
4. **Jump to Handler**: PC is set to SVC_Handler address from vector table
5. **Handler Execution**: Handler captures EXC_RETURN value
6. **Exception Return**: `bx lr` triggers exception return sequence
7. **Automatic Context Restore**: Processor pops saved registers from stack


## GDB Debug Session

### Set Breakpoints

```gdb
(gdb) break main
(gdb) break Trigger_SVC
(gdb) break SVC_Handler
```

### Examine Exception Behavior

```gdb
# Run to main
(gdb) continue

# Step into SVC trigger
(gdb) step

# Examine LR before SVC
(gdb) info registers lr

# Step into SVC instruction
(gdb) stepi

# Now you're in SVC_Handler - examine LR (contains EXC_RETURN)
(gdb) info registers lr
(gdb) x/t $lr    # Display in binary to see bit pattern

# Step through handler
(gdb) stepi
(gdb) stepi
(gdb) stepi

# After returning, examine exc_return variable
(gdb) print/x exc_return
(gdb) print/t exc_return    # Binary format

# Decode the value
# 0xFFFFFFF9 = Return to Thread mode using MSP
```

### Examine Stack Frame

```gdb
# Before SVC
(gdb) break Trigger_SVC
(gdb) continue
(gdb) info registers sp

# After entering SVC_Handler
(gdb) break SVC_Handler
(gdb) continue
(gdb) info registers sp    # Note: SP decreased (stack grew)

# Examine hardware-stacked frame
(gdb) x/8xw $sp
# You'll see: R0, R1, R2, R3, R12, LR, PC, xPSR
```

### Examine CPU Modes

```gdb
# Check IPSR (Interrupt Program Status Register)
(gdb) print/x $xpsr
(gdb) print ($xpsr & 0x1FF)    # Extract exception number

# In SVC_Handler:
# IPSR = 11 (0xB) = SVC exception active

# After return:
# IPSR = 0 = Thread mode
```

## Key Takeaways

1. **SVC is software-triggered**: Unlike hardware interrupts, SVC is deterministic
2. **EXC_RETURN encodes return context**: LR contains information about how to return
3. **Automatic stacking**: Processor saves/restores R0-R3, R12, LR, PC, xPSR
4. **Naked handlers**: Required for precise control in exception handlers
5. **Foundation for RTOS**: SVC mechanism is how system calls work in embedded RTOS

## References

- ARM Cortex-M3 Technical Reference Manual
- ARMv7-M Architecture Reference Manual (Exception Model)
- ARM Application Note 179: Cortex-M3 Embedded Software Development
- Joseph Yiu, "The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors"
