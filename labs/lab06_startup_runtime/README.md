# Lab 06: Startup and C Runtime

## Overview

This lab demonstrates the difference between minimal baremetal startup code and a full C runtime environment. While previous labs used a minimal startup that jumped directly to `main()`, this lab implements complete initialization of the `.data` and `.bss` sections, along with support for C++ constructor/destructor arrays. Understanding C runtime initialization is essential for embedded engineers working with complex applications that use global variables and C++ features.

## Learning Objectives

- Understand the complete startup sequence for Cortex-M3
- Learn how `.data` section initialization works
- Master `.bss` zero-initialization
- Understand load address vs. execution address concepts
- Learn about C++ constructor/destructor support in bare-metal systems
- Debug memory initialization with GDB
- Compare minimal vs. full runtime startup code

## Key Concepts

### Minimal vs. Full Runtime Startup

#### Minimal Startup (Labs 00-05)
```assembly
Reset_Handler:
    b main              # Jump directly to main
1:
    b 1b                # Infinite loop after main returns
```

**Limitations:**
- Global variables with initial values don't work correctly
- Uninitialized globals contain garbage (not zero)
- C++ constructors never execute
- Not compliant with C/C++ standards

#### Full Runtime Startup (Lab 06)
```assembly
Reset_Handler:
    # 1. Copy .data from FLASH to RAM
    # 2. Zero-initialize .bss
    # 3. Call SystemInit()
    # 4. Call C++ constructors (__local_init_array)
    # 5. Call main()
    # 6. Call C++ destructors (__local_cpp_fini)
    # 7. Infinite loop
```

**Benefits:**
- Global variables work as expected
- Uninitialized globals are properly zeroed
- C++ global objects are constructed/destructed
- Full C/C++ standard compliance

### Load Address vs. Execution Address

**Key Concept:** FLASH is read-only and non-volatile; RAM is read-write but volatile.

| Section | Load Address | Execution Address | Reason |
|---------|--------------|-------------------|--------|
| `.text` | FLASH | FLASH | Code can execute from ROM |
| `.rodata` | FLASH | FLASH | Read-only data stays in ROM |
| `.data` | FLASH | RAM | Must copy to RAM for read-write access |
| `.bss` | N/A | RAM | Zero-initialized at runtime |

**Linker Script Syntax:**
```ld
.data : { ... } > RAM AT > FLASH
             # Execute ↑    ↑ Load from
```

## Debug Session: Verify Memory Initialization

### Step 1: Inspect Data Copy Loop

```gdb
# Set breakpoint at copy loop
(gdb) break copy_loop
Breakpoint 2 at 0x...

# View registers
(gdb) info registers r0 r1 r2 r3
r0             0x...    # Source address (_sidata)
r1             0x...    # Destination address (_sdata)
r2             0x...    # End address (_edata)
r3             0x...    # Data being copied

# Step through loop
(gdb) stepi
(gdb) stepi

# Examine memory being copied
(gdb) x/4wx $r0    # Source (FLASH)
(gdb) x/4wx $r1    # Destination (RAM)
```

### Step 2: Inspect BSS Zero Loop

```gdb
# Set breakpoint at zero loop
(gdb) break zero_loop
Breakpoint 3 at 0x...

# View registers
(gdb) info registers r1 r2 r3
r1             0x...    # Current BSS address
r2             0x...    # End of BSS (_ebss)
r3             0x0      # Zero value

# Watch memory being zeroed
(gdb) x/8wx $r1
```

### Step 3: Verify Constructor Arrays

```gdb
# Check constructor array boundaries
(gdb) print &__init_array_start
(gdb) print &__init_array_end
(gdb) print (int)(__init_array_end - __init_array_start)

# Set breakpoint in __local_init_array
(gdb) break __local_init_array

# Step through constructor calls
(gdb) next
```

## Key Takeaways

1. **C Runtime Requirements**: C programs need proper `.data` and `.bss` initialization to work correctly
2. **Load vs. Execute**: Initialized data must be stored in FLASH but copied to RAM
3. **Zero Initialization**: C standard mandates uninitialized globals are zero
4. **Constructor Support**: Full runtime enables C++ features in bare-metal systems
5. **Startup Sequence**: Critical initialization happens before `main()` executes
6. **Memory Layout**: Linker script controls section placement and symbol generation
7. **Assembly Skills**: Low-level initialization requires assembly implementation
8. **Debugging Methodology**: Understanding startup helps debug initialization issues

## Comparison: Minimal vs. Full Runtime

| Feature | Minimal Startup | Full Runtime |
|---------|----------------|--------------|
| Code Size | ~20 bytes | ~100+ bytes |
| Initialization Time | Instant | Microseconds |
| Initialized Globals | ❌ Broken | ✅ Works |
| Uninitialized Globals | ❌ Garbage | ✅ Zero |
| C++ Constructors | ❌ Never run | ✅ Supported |
| C++ Destructors | ❌ Never run | ✅ Supported |
| Use Case | Simple demos | Production code |

## References

- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/latest/)
- [ARM C and C++ Libraries and Floating-Point Support](https://developer.arm.com/documentation/dui0378/latest/)
- [GNU Linker Scripts](https://sourceware.org/binutils/docs/ld/Scripts.html)
- [ELF File Format](https://refspecs.linuxfoundation.org/elf/elf.pdf)
- [C Runtime Initialization](https://interrupt.memfault.com/blog/zero-to-main-1)
