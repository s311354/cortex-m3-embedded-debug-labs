# Tmux + Kiro CLI Architecture

## System Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         User Entry Points                               │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  ./scripts/tmux.sh           Start tmux workspace                       │
│       │                                                                 │
│       ├── Creates 5 windows with optimized layouts                      │
│       ├── Sources .tmux.project.conf                                    │
│       └── Displays welcome banner                                       │
│                                                                         │
│  ./scripts/lab-switch.sh     Navigate between labs                      │
│       │                                                                 │
│       ├── Lists available labs                                          │
│       ├── Updates tmux panes                                            │
│       └── Changes working directory                                     │
│                                                                         │
│  ./scripts/kiro-helper.sh    AI assistance commands                     │
│       │                                                                 │
│       ├── Pre-configured Kiro prompts                                   │
│       ├── Code analysis workflows                                       │
│       └── Learning assistance                                           │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘
```

## Tmux Window Flow

┌────────────────────────────────────────────────────────────────────────┐
│                          Tmux Session: cortexm3                        │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  Window 1: Kiro                                                        │
│  ┌─────────────────────────┬──────────────────────────────────────┐   │
│  │ Pane 0: Kiro CLI       │ Pane 1: Editor/Inspector            │   │
│  │ • kiro chat            │ • vim, cat, grep                     │   │
│  │ • kiro goal            │ • Quick file inspection              │   │
│  │ • ./kiro-helper.sh     │ • Code browsing                      │   │
│  └─────────────────────────┴──────────────────────────────────────┘   │
│  [Prefix + k]                                                          │
│                                                                        │
│  Window 2: Debug                                                       │
│  ┌──────────────────────┬──────────────────────┐                      │
│  │ Pane 0: QEMU        │ Pane 1: GDB          │                      │
│  │ • make debug LAB=   │ • make gdb LAB=      │                      │
│  │ • Emulator output   │ • Debugger console   │                      │
│  ├──────────────────────┴──────────────────────┤                      │
│  │ Pane 2: Build & Test                       │                      │
│  │ • make                                      │                      │
│  │ • make dump                                 │                      │
│  └─────────────────────────────────────────────┘                      │
│  [Prefix + d]                                                          │
│                                                                        │
│  Window 3: Lab                                                         │
│  ┌────────────────────────────┬────────────────────────┐              │
│  │ Pane 0: Lab Code          │ Pane 1: Lab Build      │              │
│  │ • Edit source files        │ • Compile              │              │
│  │ • View headers             │ • Test                 │              │
│  │ • Modify linker scripts    │ • Quick builds         │              │
│  └────────────────────────────┴────────────────────────┘              │
│  [Prefix + l]                                                          │
│                                                                        │
│  Window 4: Analysis                                                    │
│  ┌──────────────────────────────────────────────────┐                 │
│  │ Pane 0: Disassembly / objdump                    │                 │
│  │ • arm-none-eabi-objdump -d                       │                 │
│  │ • Assembly analysis                              │                 │
│  ├──────────────────────────────────────────────────┤                 │
│  │ Pane 1: Inspection                               │                 │
│  │ • arm-none-eabi-readelf                          │                 │
│  │ • arm-none-eabi-nm                               │                 │
│  └──────────────────────────────────────────────────┘                 │
│  [Prefix + a]                                                          │
│                                                                        │
│  Window 5: Git                                                         │
│  ┌──────────────────────────────────────────────────┐                 │
│  │ Pane 0: Git Commands                             │                 │
│  │ • git status, diff, commit                       │                 │
│  │ • Version control workflow                       │                 │
│  └──────────────────────────────────────────────────┘                 │
│  [Prefix + g]                                                          │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘

## Development Workflow

┌─────────────────────────────────────────────────────────────────────────┐
│                        Development Lifecycle                            │
└─────────────────────────────────────────────────────────────────────────┘

    1. START                      2. LEARN                   3. IMPLEMENT
    ▼                             ▼                          ▼
┌──────────────┐          ┌──────────────────┐      ┌────────────────┐
│ Launch tmux  │          │ Analyze with     │      │ Edit code in   │
│              │  ───────>│ Kiro             │─────>│ Lab window     │
│ ./tmux.sh    │          │                  │      │                │
│              │          │ kh analyze-lab   │      │ vim main.c     │
└──────────────┘          └──────────────────┘      └────────────────┘
                                   │                          │
                                   │                          │
                                   v                          v
    6. OPTIMIZE               5. DEBUG                   4. BUILD
    ▼                             ▼                          ▼
┌──────────────┐          ┌──────────────────┐      ┌────────────────┐
│ Ask Kiro for │          │ QEMU + GDB       │      │ Compile        │
│ improvements │<─────────│                  │<─────│                │
│              │          │ make debug/gdb   │      │ make           │
│ kh optimize  │          │                  │      │                │
└──────────────┘          └──────────────────┘      └────────────────┘

## Kiro Integration Points

┌─────────────────────────────────────────────────────────────────────────┐
│                    Kiro Helper Command Categories                       │
├─────────────────────────────────────────────────────────────────────────┤
│                                                                         │
│  📊 Code Analysis                                                       │
│  ├── analyze-lab <lab>        Explain lab concepts                     │
│  ├── review-code <file>       Best practices review                    │
│  ├── explain-disasm <lab>     Assembly explanation                     │
│  └── optimize <lab>           Performance suggestions                  │
│                                                                         │
│  📚 Learning & Documentation                                            │
│  ├── debug-help               GDB command reference                    │
│  ├── register-help <reg>      ARM register details                     │
│  ├── exception-help           Exception handling guide                 │
│  └── linker-script            Memory layout analysis                   │
│                                                                         │
│  🔧 Problem Solving                                                     │
│  ├── fix-error <msg>          Compiler error diagnosis                 │
│  ├── quick <question>         Quick Q&A                                │
│  └── goal <task>              Goal-oriented implementation             │
│                                                                         │
└─────────────────────────────────────────────────────────────────────────┘

## File Structure

```
scripts/
├── tmux.sh                   Main launcher (refactored)
│   ├── Creates 5 specialized windows
│   ├── Configures pane layouts
│   ├── Sets pane titles
│   └── Sources project config
│
├── lab-switch.sh            Lab navigation utility
│   ├── Lists available labs
│   ├── Validates lab existence
│   ├── Updates tmux panes
│   └── Changes directories
│
├── kiro-helper.sh           AI assistance wrapper
│   ├── Pre-configured prompts
│   ├── Context-aware commands
│   └── Learning workflows
│
├── welcome.sh               Startup banner
│   ├── Quick reference
│   ├── Lab listing
│   └── Usage tips
│
├── TMUX_WORKFLOW.md         Comprehensive guide
│   ├── Detailed instructions
│   ├── Workflow examples
│   └── Troubleshooting
│
└── QUICK_REFERENCE.txt      One-page cheat sheet
    ├── Key bindings
    ├── Commands
    └── Tips
```

## Quick Start Commands

```bash
# 1. Launch the workspace
./scripts/tmux.sh

# 2. Switch to a lab
./scripts/lab-switch.sh 01

# 3. Get AI help
./scripts/kiro-helper.sh analyze-lab lab01_core_registers

# 4. Build and debug
make debug LAB=lab01_core_registers  # In Debug window, pane 0
make gdb LAB=lab01_core_registers    # In Debug window, pane 1

# 5. Analyze
make dump LAB=lab01_core_registers   # In Analysis window
```
