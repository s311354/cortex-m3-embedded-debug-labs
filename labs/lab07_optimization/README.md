# Lab 07: Compiler Optimization and Performance on Cortex-M3

## Overview

This lab explores compiler optimization behavior, memory access patterns, and low-level performance considerations specific to ARM Cortex-M3 embedded systems. Through seven distinct practices, you'll learn how the compiler translates C code to ARM Thumb-2 assembly and how to write correct, efficient bare-metal code.

## Learning Objectives

- Understand the `volatile` keyword and when it's essential
- Differentiate between compiler barriers and hardware memory barriers
- Analyze the impact of optimization levels on code generation
- Use the DWT (Data Watchpoint and Trace) cycle counter for performance measurement
- Compare function call overhead: normal vs inline functions
- Understand memory access hierarchy: registers vs stack vs global
- Verify that CMSIS intrinsics compile to efficient assembly

## Practices Overview

### Practice 1: Volatile vs Non-Volatile Variables

**Purpose**: Understand when `volatile` is necessary to prevent incorrect optimization.

**What to observe**: At `-O0`, both produce identical assembly. At `-O2` or `-O3`, the non-volatile version may cache the value in a register, creating an infinite loop if the variable is modified by an ISR or hardware.

### Practice 2: Compiler Barrier vs DMB

**Purpose**: Distinguish between compile-time and runtime memory ordering.

**Key differences**:
- **No barrier**: Compiler may reorder stores; CPU may reorder at runtime
- **Compiler barrier**: Prevents compiler reordering only (no hardware instruction)
- **DMB**: Inserts ARM `dmb` instruction, enforcing memory ordering at runtime (essential for DMA, multi-core, caches)

### Practice 3: Optimization Level Impact

**Purpose**: See how optimization transforms loop code.

**What to observe**:
- `-O0`: Full loop with stack operations (slow, debuggable)
- `-O1`: Register allocation, reduced memory access
- `-O2`: Loop unrolling, strength reduction
- `-O3`: May compute result as constant (499,500) at compile time

### Practice 4: DWT Cycle Counter

**Purpose**: Learn to use Cortex-M3 hardware performance counters.

**Hardware involved**:
- **CoreDebug->DEMCR**: Debug Exception and Monitor Control Register
- **DWT->CTRL**: Data Watchpoint and Trace Control
- **DWT->CYCCNT**: 32-bit cycle counter (wraps at 2^32)

### Practice 5: Function Inlining

**Purpose**: Understand function call overhead.

**What to observe**:
- `add_normal`: Requires push, stack frame setup, bx lr (4-8 cycles overhead)
- `add_inline`: Code embedded at call site (0 cycles overhead)

**When to inline**: Small, frequently called functions (but watch code size growth)

### Practice 6: Memory Access Patterns

**Purpose**: Compare access performance for different storage classes.

**Performance hierarchy** (Cortex-M3 without cache):
1. **Register**: Fastest (0 cycle access)
2. **Stack**: 1-2 cycles (SP-relative addressing)
3. **Global**: 2-3 cycles (requires PC-relative load then dereference)

### Practice 7: CMSIS vs Raw Assembly

**Purpose**: Verify CMSIS intrinsics are zero-overhead abstractions.

**Result**: All pairs produce identical assembly. CMSIS provides portability without performance cost.

## Experiments

### Experiment 1: Volatile Effect at Different Optimization Levels

```bash
# Compile at O0
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O0 -DPRACTICE1 -c main.c -o main_O0.o
arm-none-eabi-objdump -d main_O0.o > disasm_O0.txt

# Compile at O2
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -O2 -DPRACTICE1 -c main.c -o main_O2.o
arm-none-eabi-objdump -d main_O2.o > disasm_O2.txt

# Compare the loops
diff -u disasm_O0.txt disasm_O2.txt
```

**Expected**: At `-O2`, `test_nonvolatile()` may optimize to an infinite loop, while `test_volatile()` always re-reads from memory.

### Experiment 2: Measure Cycle Counts

```bash
make debug  # Terminal 1

# Terminal 2
gdb-multiarch lab07_optimization.elf -ex "target remote :1234"

(gdb) break dwt_init
(gdb) continue
(gdb) finish

# Measure delay_loop
(gdb) set var DWT->CYCCNT = 0
(gdb) break delay_loop
(gdb) continue
(gdb) finish
(gdb) print DWT->CYCCNT
```

### Experiment 3: Compare DMB vs No Barrier

```bash
# Build with barriers enabled
make clean
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -g3 -O0 -DPRACTICE2 -c main.c -o main.o
arm-none-eabi-objdump -d main.o | grep -A 15 "without_barrier\|dmb_barrier"
```

Look for the `dmb sy` instruction in `dmb_barrier()` but not in `without_barrier()`.

### Experiment 4: Function Inlining Impact

```bash
# Compile with inline test
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -g3 -O1 -DPRACTICE5 -c main.c -o main.o

# Check if add_inline actually gets inlined
arm-none-eabi-objdump -d main.o | grep "add_inline"
# (Should not appear as a function at O1 or higher)

# Check add_normal exists
arm-none-eabi-objdump -d main.o | grep -A 10 "add_normal"
```

## Key ARM Cortex-M3 Features Demonstrated

### Instructions

- `cpsid i` / `cpsie i`: Change Processor State (disable/enable interrupts)
- `dmb sy`: Data Memory Barrier, system-wide
- `ldr` / `str`: Load/Store register
- `bcc`: Branch if Carry Clear (unsigned less than)
- `bx lr`: Branch and exchange to link register (return)

### Registers

- **R0-R12**: General purpose (R0-R3 for arguments/return values)
- **R7**: Frame pointer (in these examples)
- **SP (R13)**: Stack pointer
- **LR (R14)**: Link register (return address)
- **PC (R15)**: Program counter

### Memory-Mapped Peripherals

- **CoreDebug (0xE000EDF0)**: Debug control
  - `DEMCR[24]`: TRCENA - Enable trace and debug blocks
- **DWT (0xE0001000)**: Data Watchpoint and Trace
  - `CTRL[0]`: CYCCNTENA - Enable cycle counter
  - `CYCCNT`: Current cycle count

## When to Use What

| Scenario | Solution |
|----------|----------|
| Variable modified by ISR | `volatile` |
| Shared data with DMA | `volatile` + `__DMB()` |
| Prevent compiler reordering only | `asm volatile("" ::: "memory")` |
| Multi-core synchronization | `__DMB()` or `__DSB()` |
| Small hot function | `static inline` |
| Performance measurement | DWT cycle counter |
| Portable assembly | CMSIS intrinsics |

## Further Reading

- ARM Cortex-M3 Technical Reference Manual
- ARM Architecture Reference Manual ARMv7-M
- GCC ARM Options: https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html
- CMSIS Core Documentation

---

**Tips for Embedded Engineers**:
- Always examine disassembly for performance-critical code
- Use `volatile` sparingly but correctly
- Understand your target's memory model (Cortex-M3 is weakly ordered)
- Profile before optimizing - measure with DWT
- CMSIS provides good abstractions - use them
