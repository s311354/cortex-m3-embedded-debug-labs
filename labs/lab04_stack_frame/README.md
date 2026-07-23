# Lab 04: Exception Stack Frame

## Overview

This lab demonstrates the **hardware exception stack frame** mechanism in the ARM Cortex-M3 processor. When an exception occurs, the processor automatically saves context by pushing eight registers onto the stack. Understanding this mechanism is crucial for exception handling, fault analysis, and RTOS development.

## Learning Objectives

- Understand hardware automatic stacking during exception entry
- Learn the exception stack frame layout (8 registers)
- Capture and inspect the EXC_RETURN value
- Use naked functions for precise register control
- Debug exception handlers using stack frame analysis

## Key Concepts

### Hardware Exception Stack Frame

When the Cortex-M3 takes an exception (interrupt, fault, or system call), it **automatically** pushes 8 registers onto the active stack:

```
High Address
┌──────────────┐
│   xPSR       │  [7]  Program Status Register
├──────────────┤
│   PC         │  [6]  Return Address
├──────────────┤
│   LR         │  [5]  Link Register
├──────────────┤
│   R12        │  [4]  General Purpose
├──────────────┤
│   R3         │  [3]  Argument/Result
├──────────────┤
│   R2         │  [2]  Argument
├──────────────┤
│   R1         │  [1]  Argument
├──────────────┤
│   R0         │  [0]  Argument/Result
└──────────────┘ <- Stack Pointer after stacking
Low Address
```

### EXC_RETURN

During exception entry, the Link Register (LR) is loaded with a special value called **EXC_RETURN**. This magic value controls exception return behavior:

| EXC_RETURN Value | Return Stack | Return Mode |
|------------------|--------------|-------------|
| `0xFFFFFFF1`     | MSP          | Handler Mode |
| `0xFFFFFFF9`     | MSP          | Thread Mode |
| `0xFFFFFFFD`     | PSP          | Thread Mode |

The processor uses this value to restore context and switch modes on exception return.

## Debugging Exercise

### 1. Set Breakpoint After Exception

```gdb
(gdb) break main
(gdb) continue
(gdb) break SVC_Handler_C
(gdb) continue
```

### 2. Inspect Stack Frame

```gdb
(gdb) print/x dump[0]
$1 = 0x11111111      # R0
(gdb) print/x dump[1]
$2 = 0x22222222      # R1
(gdb) print/x dump[2]
$3 = 0x33333333      # R2
(gdb) print/x dump[3]
$4 = 0x44444444      # R3
(gdb) print/x dump[6]
$5 = 0x00000d6      # PC (return address)
(gdb) print/x dump[7]
$6 = 0x41000200      # xPSR
```

### 3. Examine EXC_RETURN

```gdb
(gdb) print/x exc_return
$7 = 0xfffffff9      # Return to Thread mode using MSP
```

**Bit Analysis:**
- Bit[31:4]: `0xFFFFFFF` (fixed pattern)
- Bit[3]: `1` = return to Thread mode, `0` = Handler mode
- Bit[2]: `0` = use MSP, `1` = use PSP
- Bit[1]: Reserved
- Bit[0]: Reserved

### 4. Disassemble and Verify

```gdb
(gdb) disassemble Trigger_SVC
```

Look for the `svc #0` instruction and note the PC value.

```gdb
(gdb) x/8wx $sp
```

Compare with the `dump[]` array contents.

## Key Takeaways

✓ **Hardware automation**: Exception stacking is atomic and automatic  
✓ **Fixed layout**: 8 registers in a defined order  
✓ **EXC_RETURN**: Special LR value controls exception return  
✓ **Naked functions**: Required for low-level exception handling  
✓ **Debug technique**: Stack frame inspection is essential for fault analysis  

## References

- ARM®v7-M Architecture Reference Manual (Section B1.5.6 - Exception Entry)
- ARM Cortex-M3 Technical Reference Manual
- ARM Procedure Call Standard (AAPCS)
- Joseph Yiu, "The Definitive Guide to ARM Cortex-M3 and Cortex-M4 Processors"
