#!/usr/bin/env bash
#
# Display welcome message and quick tips
#

cat << 'EOF'

   ╔═══════════════════════════════════════════════════════════════════════╗
   ║                                                                       ║
   ║            🤖 Cortex-M3 Embedded Debug Labs + Kiro CLI                ║
   ║                                                                       ║
   ╟───────────────────────────────────────────────────────────────────────╢
   ║  📚 Quick Start:                                                      ║
   ║     • Switch labs:  ./scripts/lab-switch.sh 01                        ║
   ║     • Kiro help:    ./scripts/kiro-helper.sh                          ║
   ║     • Quick ref:    cat scripts/QUICK_REFERENCE.txt                   ║
   ║                                                                       ║
   ║  🪟 Windows (Prefix = Ctrl+b):                                        ║
   ║     Prefix + k  → Kiro    (AI assistant)                             ║
   ║     Prefix + d  → Debug   (QEMU + GDB)                               ║
   ║     Prefix + l  → Lab     (Workspace)                                ║
   ║     Prefix + a  → Analysis (objdump)                                 ║
   ║     Prefix + g  → Git     (Version control)                          ║
   ║                                                                       ║
   ║  ⚡ Quick Actions:                                                     ║
   ║     Prefix + b  → Quick build                                        ║
   ║     Prefix + r  → Quick run                                          ║
   ║     Prefix + K  → Ask Kiro                                           ║
   ║                                                                       ║
   ║  🔧 Example Workflow:                                                 ║
   ║     1. Switch to a lab: ./scripts/lab-switch.sh 01                    ║
   ║     2. Analyze code:    kh analyze-lab lab01_core_registers           ║
   ║     3. Build & debug:   make && make debug LAB=lab01_...             ║
   ║     4. Connect GDB:     make gdb LAB=lab01_...                       ║
   ║                                                                       ║
   ║  💡 Kiro Helper (kh):                                                 ║
   ║     kh analyze-lab <lab>      - Explain lab code                     ║
   ║     kh register-help CONTROL  - Explain register                     ║
   ║     kh debug-help             - GDB reference                        ║
   ║     kh fix-error "<msg>"      - Debug errors                         ║
   ║     kh goal "<task>"          - Start goal task                      ║
   ║                                                                       ║
   ╚═══════════════════════════════════════════════════════════════════════╝

EOF

# Show available labs
echo "  📋 Available Labs:"
cd "$(dirname "$0")/.." || exit 1
for lab_dir in labs/lab*; do
    if [ -d "${lab_dir}" ]; then
        lab_name=$(basename "${lab_dir}")
        printf "     • %-30s\n" "${lab_name}"
    fi
done | head -n 8

if [ "$(find labs -maxdepth 1 -type d -name "lab*" | wc -l)" -gt 8 ]; then
    echo "     ... and more (run 'make list' to see all)"
fi

echo ""
echo "  ℹ️  Full docs: cat scripts/TMUX_WORKFLOW.md"
echo ""
