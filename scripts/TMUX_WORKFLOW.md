# Tmux + Kiro CLI Workflow for Cortex-M3 Labs

This directory contains scripts to set up an efficient tmux workspace optimized for embedded development with Kiro CLI integration.

## Quick Start

```bash
# Start tmux session
./scripts/tmux.sh

# Start with a specific lab
./scripts/tmux.sh cortexm3 lab01_core_registers

# Inside tmux, switch to a different lab
./scripts/lab-switch.sh lab03_svc_exception
# Or use lab number shorthand
./scripts/lab-switch.sh 03

# Use Kiro helper commands
./scripts/kiro-helper.sh analyze-lab lab01_core_registers
```

## Workspace Layout

### Window 1: Kiro (AI Assistant)
```
┌─────────────────────────────┬──────────────────────────┐
│                             │                          │
│   Kiro CLI                  │   Editor/Inspector       │
│   - AI assistance           │   - Quick edits          │
│   - Code analysis           │   - File inspection      │
│   - Problem solving         │   - grep/find            │
│                             │                          │
└─────────────────────────────┴──────────────────────────┘
```

**Usage:**
- Left pane: Run `kiro chat`, `kiro goal`, or use `./scripts/kiro-helper.sh`
- Right pane: Quick file inspection, vim, cat, grep

### Window 2: Debug (Development)
```
┌──────────────────────┬──────────────────────┐
│                      │                      │
│   QEMU               │   GDB                │
│   make debug LAB=... │   make gdb LAB=...   │
│                      │                      │
├──────────────────────┴──────────────────────┤
│                                             │
│   Build & Test                              │
│   make, objdump, readelf                    │
│                                             │
└─────────────────────────────────────────────┘
```

**Typical workflow:**
1. Pane 0 (QEMU): `make debug LAB=lab01_core_registers`
2. Pane 1 (GDB): `make gdb LAB=lab01_core_registers`
3. Pane 2 (Build): `make lab01_core_registers`

### Window 3: Lab (Current Lab Workspace)
```
┌──────────────────────────────┬─────────────────┐
│                              │                 │
│   Lab Code                   │   Build Output  │
│   - Edit source files        │   - Compile     │
│   - View headers             │   - Test        │
│   - Inspect linker scripts   │   - Run         │
│                              │                 │
└──────────────────────────────┴─────────────────┘
```

### Window 4: Analysis (Binary Inspection)
```
┌─────────────────────────────────────────────┐
│   Disassembly / objdump                     │
│   arm-none-eabi-objdump -d lab.elf          │
├─────────────────────────────────────────────┤
│   Symbol Table / Memory Map                 │
│   arm-none-eabi-readelf -a lab.elf          │
└─────────────────────────────────────────────┘
```

### Window 5: Git (Version Control)
```
┌─────────────────────────────────────────────┐
│                                             │
│   Git Commands                              │
│   - git status                              │
│   - git diff                                │
│   - git commit                              │
│                                             │
└─────────────────────────────────────────────┘
```

## Key Bindings

### Window Navigation
- `Prefix + k` - Switch to **Kiro** window
- `Prefix + d` - Switch to **Debug** window
- `Prefix + l` - Switch to **Lab** window
- `Prefix + a` - Switch to **Analysis** window
- `Prefix + g` - Switch to **Git** window
- `Prefix + 1-5` - Switch by number

### Quick Actions
- `Prefix + b` - Quick **build** (prompts for lab name)
- `Prefix + r` - Quick **run** (prompts for lab name)
- `Prefix + S` - **Sync** panes (toggle)

### Pane Management
- `Prefix + |` - Split horizontally
- `Prefix + -` - Split vertically
- `Prefix + H/J/K/L` - Resize pane (vim-style)
- `Prefix + o` - Cycle through panes

## Kiro Helper Commands

The `kiro-helper.sh` script provides convenient commands for common Kiro CLI workflows:

### Code Analysis
```bash
./scripts/kiro-helper.sh analyze-lab lab01_core_registers
./scripts/kiro-helper.sh review-code labs/lab09_uart_polling/uart.c
./scripts/kiro-helper.sh explain-disasm lab03_svc_exception
./scripts/kiro-helper.sh optimize lab07_optimization
```

### Learning & Help
```bash
./scripts/kiro-helper.sh debug-help
./scripts/kiro-helper.sh register-help CONTROL
./scripts/kiro-helper.sh exception-help
./scripts/kiro-helper.sh linker-script
```

### Problem Solving
```bash
./scripts/kiro-helper.sh fix-error "undefined reference to main"
./scripts/kiro-helper.sh quick "What is EXC_RETURN in Cortex-M3?"
./scripts/kiro-helper.sh goal "implement UART interrupt handler"
```

## Typical Development Workflow

### 1. Start a Lab
```bash
# Start tmux with specific lab
./scripts/tmux.sh cortexm3 lab01_core_registers

# Or switch inside tmux
./scripts/lab-switch.sh 01
```

### 2. Understand the Lab
```bash
# In Kiro window (Prefix + k)
./scripts/kiro-helper.sh analyze-lab lab01_core_registers
```

### 3. Build and Debug
```bash
# In Debug window, pane 0 (Prefix + d)
make debug LAB=lab01_core_registers

# In Debug window, pane 1
make gdb LAB=lab01_core_registers

# GDB commands
(gdb) break main
(gdb) continue
(gdb) info registers
(gdb) print g_control
```

### 4. Analyze Assembly
```bash
# In Analysis window (Prefix + a)
make dump LAB=lab01_core_registers | less

# Check memory layout
arm-none-eabi-readelf -S labs/lab01_core_registers/lab01_core_registers.elf
```

### 5. Ask Kiro for Help
```bash
# In Kiro window
kiro chat "Explain the CONTROL register bits shown in this code"

# Or use helper
./scripts/kiro-helper.sh register-help CONTROL
```

### 6. Modify and Test
```bash
# In Lab window (Prefix + l)
vim main.c

# Build
make

# Test with QEMU (switch to Debug window)
make run LAB=lab01_core_registers
```

## Advanced Tips

### Pane Synchronization
Run the same command in multiple panes:
```bash
Prefix + S  # Toggle sync mode
# Type command - appears in all panes
Prefix + S  # Toggle off
```

### Copy Mode (for Kiro output)
```bash
Prefix + [        # Enter copy mode
v                 # Start selection (vim-style)
y                 # Copy selection
Prefix + ]        # Paste
```

### Session Management
```bash
# Detach from session
Prefix + d

# List sessions
tmux ls

# Attach to session
tmux attach -t cortexm3

# Kill session
tmux kill-session -t cortexm3
```
## References

- [tmux Documentation](https://github.com/tmux/tmux/wiki)
- [Kiro CLI Documentation](https://docs.kiro.ai)
- ARM Cortex-M3 Technical Reference Manual
- GNU Arm Embedded Toolchain Documentation
