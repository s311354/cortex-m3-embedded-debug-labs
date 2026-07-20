# Scripts Directory

This directory contains scripts for an optimized Cortex-M3 embedded development workflow with tmux and Kiro CLI integration.

## 📁 Files

### Executable Scripts

| Script | Purpose | Usage |
|--------|---------|-------|
| `tmux.sh` | Launch tmux workspace | `./tmux.sh [session] [lab]` |
| `lab-switch.sh` | Switch between labs | `./lab-switch.sh <lab_name or number>` |
| `kiro-helper.sh` | Kiro CLI wrapper | `./kiro-helper.sh <command> [args]` |
| `welcome.sh` | Display welcome banner | `./welcome.sh` |

## 🚀 Quick Start

```bash
# 1. Start tmux workspace
./tmux.sh

# 2. Start with specific lab
./tmux.sh cortexm3 lab01_core_registers

# 3. Switch labs (inside tmux)
./lab-switch.sh 03

# 4. Get help from Kiro
./kiro-helper.sh analyze-lab lab03_svc_exception
```
## 🎯 Use Cases

### Learning ARM Cortex-M3
```bash
# Ask Kiro about registers
./kiro-helper.sh register-help CONTROL

# Explain exception handling
./kiro-helper.sh exception-help

# Get GDB help
./kiro-helper.sh debug-help
```

### Developing a Lab
```bash
# Start workspace with lab
./tmux.sh cortexm3 lab09_uart_polling

# Analyze the lab code
./kiro-helper.sh analyze-lab lab09_uart_polling

# Build and debug
# (In Debug window)
make debug LAB=lab09_uart_polling
make gdb LAB=lab09_uart_polling
```

### Debugging Issues
```bash
# Get error explanation
./kiro-helper.sh fix-error "undefined reference to uart_init"

# Explain disassembly
./kiro-helper.sh explain-disasm lab09_uart_polling

# Review code
./kiro-helper.sh review-code labs/lab09_uart_polling/uart.c
```

### Starting New Features
```bash
# Goal-oriented task
./kiro-helper.sh goal "implement UART interrupt handler with ringbuffer"

# Quick questions
./kiro-helper.sh quick "What is the difference between MSP and PSP?"
```

## 🔧 Script Details

### tmux.sh
**Purpose:** Launch a tmux workspace optimized for embedded development

**Usage:**
```bash
./tmux.sh                          # Default session
./tmux.sh cortexm3                 # Named session
./tmux.sh cortexm3 lab01_core_registers  # With lab
```

### lab-switch.sh
**Purpose:** Quickly navigate between labs

**Usage:**
```bash
./lab-switch.sh                    # List labs
./lab-switch.sh 01                 # Switch to lab01
./lab-switch.sh lab03_svc_exception  # Full name
```

### kiro-helper.sh
**Purpose:** Pre-configured Kiro CLI commands for embedded development

**Commands:**

**Code Analysis:**
- `analyze-lab <lab>` - Explain lab code and concepts
- `review-code <file>` - Code review for best practices
- `explain-disasm <lab>` - Explain assembly output
- `optimize <lab>` - Get optimization suggestions

**Learning:**
- `debug-help` - GDB command reference
- `register-help <reg>` - Explain ARM registers
- `exception-help` - Exception handling guide
- `linker-script` - Analyze linker script

**Problem Solving:**
- `fix-error <msg>` - Debug compiler errors
- `quick <question>` - Quick questions
- `goal <task>` - Start goal-oriented task

**Usage:**
```bash
./kiro-helper.sh                   # Show help
./kiro-helper.sh analyze-lab lab01_core_registers
./kiro-helper.sh register-help CONTROL
```

### welcome.sh
**Purpose:** Display welcome banner with quick tips

**Usage:**
```bash
./welcome.sh
```

## 🎨 Tmux Layout

```
Window 1: Kiro          Window 2: Debug         Window 3: Lab
┌──────────┬────────┐   ┌──────┬──────┐        ┌──────────┬────────┐
│ Kiro CLI │ Editor │   │ QEMU │ GDB  │        │ Lab Code │ Build  │
│          │        │   ├──────┴──────┤        │          │        │
└──────────┴────────┘   │ Build/Test  │        └──────────┴────────┘
                        └─────────────┘

Window 4: Analysis      Window 5: Git
┌────────────────────┐  ┌────────────────────┐
│ Disassembly        │  │ Git Commands       │
├────────────────────┤  └────────────────────┘
│ Memory/Symbols     │
└────────────────────┘
```

## ⌨️ Key Bindings

**Window Navigation** (Prefix = Ctrl+b):
- `Prefix + k` → Kiro
- `Prefix + d` → Debug
- `Prefix + l` → Lab
- `Prefix + a` → Analysis
- `Prefix + g` → Git

**Quick Actions:**
- `Prefix + b` → Quick build
- `Prefix + r` → Quick run
- `Prefix + S` → Sync panes

**Pane Management:**
- `Prefix + |` → Split horizontal
- `Prefix + -` → Split vertical
- `Prefix + H/J/K/L` → Resize pane

## 💡 Tips

## 🔍 Examples

### Example 1: Learning about registers
```bash
./tmux.sh
./lab-switch.sh 01
./kiro-helper.sh register-help CONTROL
# Kiro explains CONTROL register bits
```

### Example 2: Debugging a lab
```bash
./tmux.sh cortexm3 lab03_svc_exception
# In Debug window, pane 0:
make debug LAB=lab03_svc_exception
# In Debug window, pane 1:
make gdb LAB=lab03_svc_exception
# In GDB:
break SVC_Handler
continue
```

### Example 3: Code review
```bash
./kiro-helper.sh review-code labs/lab09_uart_polling/uart.c
# Kiro reviews code for:
# - Memory safety
# - Register access correctness
# - Embedded best practices
```

### Example 4: Starting new work
```bash
./lab-switch.sh 11
./kiro-helper.sh analyze-lab lab11_uart_ringbuffer
./kiro-helper.sh goal "add overflow detection to ringbuffer"
```

## 📚 Further Reading

- [Tmux Documentation](https://github.com/tmux/tmux/wiki)
- [Kiro CLI Documentation](https://docs.kiro.ai)
- [ARM Cortex-M3 Technical Reference](https://developer.arm.com)
