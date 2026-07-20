#!/usr/bin/env bash
#
# Kiro Workflow Helper - Common kiro-cli commands for embedded development
#

set -e

COMMAND="$1"
shift || true

case "${COMMAND}" in
    analyze-lab)
        LAB="${1:-}"
        if [ -z "${LAB}" ]; then
            echo "Usage: $0 analyze-lab <lab_name>"
            exit 1
        fi
        echo "Analyzing lab: ${LAB}"
        kiro-cli chat "Analyze the ${LAB} lab code and explain what it demonstrates about Cortex-M3"
        ;;
        
    explain-disasm)
        LAB="${1:-}"
        if [ -z "${LAB}" ]; then
            echo "Usage: $0 explain-disasm <lab_name>"
            exit 1
        fi
        echo "Explaining disassembly for: ${LAB}"
        ELF_FILE="labs/${LAB}/${LAB}.elf"
        if [ -f "${ELF_FILE}" ]; then
            arm-none-eabi-objdump -d "${ELF_FILE}" > /tmp/disasm.txt
            kiro-cli chat "Explain this ARM Cortex-M3 disassembly, focusing on the Thumb-2 instructions: $(cat /tmp/disasm.txt | head -n 50)"
        else
            echo "Error: ELF file not found. Build the lab first: make ${LAB}"
        fi
        ;;
        
    debug-help)
        echo "Getting GDB debugging help from Kiro..."
        kiro-cli chat "I'm debugging an ARM Cortex-M3 application with GDB and QEMU. What are the essential GDB commands for embedded debugging? Focus on: breakpoints, examining registers, stack frames, and memory inspection."
        ;;
        
    register-help)
        REGISTER="${1:-}"
        if [ -z "${REGISTER}" ]; then
            echo "Usage: $0 register-help <register_name>"
            echo "Example: $0 register-help CONTROL"
            exit 1
        fi
        kiro-cli chat "Explain the ARM Cortex-M3 ${REGISTER} register in detail: its purpose, bit fields, and how it's used in ARMv7-M architecture."
        ;;
        
    exception-help)
        kiro-cli chat "Explain ARM Cortex-M3 exception handling: vector table, exception priorities, NVIC, and the hardware exception stack frame. Include practical examples."
        ;;
        
    linker-script)
        kiro-cli chat "Analyze the Cortex-M3 linker script at platform/baremetal/linker.ld and explain the memory layout, sections, and startup sequence"
        ;;
        
    review-code)
        FILE="${1:-}"
        if [ -z "${FILE}" ]; then
            echo "Usage: $0 review-code <file_path>"
            exit 1
        fi
        kiro-cli chat "Review the embedded C code at ${FILE} for Cortex-M3. Check for: memory safety, register access correctness, interrupt handling, and embedded best practices"
        ;;
        
    fix-error)
        ERROR_MSG="$*"
        if [ -z "${ERROR_MSG}" ]; then
            echo "Usage: $0 fix-error <error_message>"
            echo "Paste the compiler or linker error you're seeing"
            exit 1
        fi
        kiro-cli chat "I'm getting this error while building an ARM Cortex-M3 embedded project: ${ERROR_MSG}. How do I fix it?"
        ;;
        
    optimize)
        LAB="${1:-}"
        if [ -z "${LAB}" ]; then
            echo "Usage: $0 optimize <lab_name>"
            exit 1
        fi
        kiro-cli chat "Review the ${LAB} code and suggest optimizations for: code size, performance, and memory usage on Cortex-M3"
        ;;
        
    goal)
        TASK="$*"
        if [ -z "${TASK}" ]; then
            echo "Usage: $0 goal <task_description>"
            echo "Example: $0 goal implement a UART ringbuffer with interrupt handling"
            exit 1
        fi
        kiro-cli goal "${TASK}"
        ;;
        
    quick)
        QUESTION="$*"
        if [ -z "${QUESTION}" ]; then
            echo "Usage: $0 quick <question>"
            exit 1
        fi
        kiro-cli chat "${QUESTION}"
        ;;
        
    *)
        cat <<EOF
╔══════════════════════════════════════════════════════════════════════════╗
║               Kiro Workflow Helper - Cortex-M3 Labs                      ║
╟──────────────────────────────────────────────────────────────────────────╢
║  Code Analysis:                                                          ║
║    analyze-lab <lab>          - Analyze and explain a lab                ║
║    review-code <file>         - Review code for best practices           ║
║    explain-disasm <lab>       - Explain disassembly output               ║
║    optimize <lab>             - Get optimization suggestions             ║
║                                                                           ║
║  Learning & Help:                                                        ║
║    debug-help                 - GDB debugging commands                   ║
║    register-help <reg>        - Explain specific register                ║
║    exception-help             - Exception handling overview              ║
║    linker-script              - Analyze linker script                    ║
║                                                                           ║
║  Problem Solving:                                                        ║
║    fix-error <msg>            - Debug compiler/linker errors             ║
║    quick <question>           - Quick question to Kiro                   ║
║    goal <task>                - Start a goal-oriented task               ║
║                                                                           ║
║  Examples:                                                               ║
║    $0 analyze-lab lab01_core_registers                                   ║
║    $0 register-help CONTROL                                              ║
║    $0 fix-error "undefined reference to main"                            ║
║    $0 goal "add UART interrupt handler with ringbuffer"                  ║
╚══════════════════════════════════════════════════════════════════════════╝
EOF
        ;;
esac
