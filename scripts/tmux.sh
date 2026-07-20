#!/usr/bin/env bash

set -e

SESSION="${1:-cortexm3}"
LAB="${2:-}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# Helper function to set pane title
set_pane_title() {
    local pane="$1"
    local title="$2"
    tmux select-pane -t "${pane}" -T "${title}"
}

# If session exists, attach to it
if tmux has-session -t "${SESSION}" 2 >/dev/null; then
    echo "Session '${SESSION}' already exists. Attaching..."
    exec tmux attach-session -t "${SESSION}"
fi

#################################################
# Create Session with Kiro Window
#################################################
tmux new-session \
    -d \
    -c "${ROOT}" \
    -s "${SESSION}" \
    -n Kiro

# Split for side-by-side: Kiro | Editor
tmux split-window \
    -h \
    -t "${SESSION}:Kiro" \
    -c "${ROOT}"

# Set main-pane-width to give editor more space (60% for editor)
tmux select-layout -t "${SESSION}:Kiro" main-vertical

# Left: Kiro CLI
set_pane_title "${SESSION}:Kiro.0" "Kiro AI"
sleep 0.5
tmux send-keys -t "${SESSION}:Kiro.0" "# Kiro CLI ready" C-m

# Right: Quick Editor/Inspector
set_pane_title "${SESSION}:Kiro.1" "Editor"

#################################################
# Window 2: Debug (3-pane: QEMU | GDB | Build)
#################################################
tmux new-window \
    -t "${SESSION}" \
    -n Debug \
    -c "${ROOT}"

# Split horizontally: Top | Bottom
tmux split-window \
    -v \
    -t "${SESSION}:Debug" \
    -c "${ROOT}" \
    -p 30

# Split top pane vertically: QEMU | GDB
tmux select-pane -t "${SESSION}:Debug.0"
tmux split-window \
    -h \
    -t "${SESSION}:Debug.0" \
    -c "${ROOT}"

# Pane 0: QEMU
sleep 0.5
set_pane_title "${SESSION}:Debug.0" "QEMU"
tmux send-keys -t "${SESSION}:Debug.0" "# QEMU emulator" C-m
if [ -n "${LAB}" ]; then
    tmux send-keys -t "${SESSION}:Debug.0" "# make debug LAB=${LAB}" C-m
else
    tmux send-keys -t "${SESSION}:Debug.0" "# make debug LAB=lab00_cross_compile" C-m
fi

# Pane 1: GDB
set_pane_title "${SESSION}:Debug.1" "GDB"
sleep 0.5
tmux send-keys -t "${SESSION}:Debug.1" "# GDB debugger" C-m
if [ -n "${LAB}" ]; then
    tmux send-keys -t "${SESSION}:Debug.1" "# make gdb LAB=${LAB}" C-m
else
    tmux send-keys -t "${SESSION}:Debug.1" "# make gdb LAB=lab00_cross_compile" C-m
fi

# Pane 2: Build/Test
set_pane_title "${SESSION}:Debug.2" "Build"
sleep 0.5
tmux send-keys -t "${SESSION}:Debug.2" "# Build and analyze" C-m
tmux send-keys -t "${SESSION}:Debug.2" "make list" C-m

#################################################
# Window 3: Lab (Full workspace for current lab)
#################################################
tmux new-window \
    -t "${SESSION}" \
    -n Lab \
    -c "${ROOT}"

# Split: Code | Terminal
tmux split-window \
    -h \
    -t "${SESSION}:Lab" \
    -c "${ROOT}" \
    -p 40

set_pane_title "${SESSION}:Lab.0" "Lab-Work"
set_pane_title "${SESSION}:Lab.1" "Lab-Build"
sleep 0.5
if [ -n "${LAB}" ]; then
    LAB_PATH="${ROOT}/labs/${LAB}"
    if [ -d "${LAB_PATH}" ]; then
        tmux send-keys -t "${SESSION}:Lab.0" "cd ${LAB_PATH}" C-m
        tmux send-keys -t "${SESSION}:Lab.0" "ls -la" C-m
	sleep 0.5
        tmux send-keys -t "${SESSION}:Lab.1" "# make" C-m
    fi
else
    tmux send-keys -t "${SESSION}:Lab.0" "cd labs" C-m
    tmux send-keys -t "${SESSION}:Lab.0" "ls -la" C-m
fi

#################################################
# Window 4: Analysis (objdump, readelf, etc.)
#################################################
tmux new-window \
    -t "${SESSION}" \
    -n Analysis \
    -c "${ROOT}"

# Split horizontally for multiple analysis views
tmux split-window \
    -v \
    -t "${SESSION}:Analysis" \
    -c "${ROOT}"

set_pane_title "${SESSION}:Analysis.0" "Disassembly"
set_pane_title "${SESSION}:Analysis.1" "Inspect"
sleep 0.5
tmux send-keys -t "${SESSION}:Analysis.0" "# objdump, readelf" C-m
tmux send-keys -t "${SESSION}:Analysis.0" "# make dump LAB=<lab_name>" C-m
sleep 0.5
tmux send-keys -t "${SESSION}:Analysis.1" "# arm-none-eabi-readelf -a <elf>" C-m
tmux send-keys -t "${SESSION}:Analysis.1" "# arm-none-eabi-nm -S <elf>" C-m

#################################################
# Window 5: Git
#################################################
tmux new-window \
    -t "${SESSION}" \
    -n Git \
    -c "${ROOT}"

set_pane_title "${SESSION}:Git.0" "Git"
sleep 0.5
tmux send-keys -t "${SESSION}:Git.0" "git status" C-m

#################################################
# Set key bindings for quick navigation
#################################################
# Bind keys for quick lab switching
tmux bind-key -T prefix L command-prompt -p "Lab:" "send-keys -t ${SESSION}:Lab.0 'cd ${ROOT}/labs/lab%% && ls' C-m"

#################################################
# Configure pane borders and status
#################################################
tmux set-option -t "${SESSION}" -g pane-border-status top
tmux set-option -t "${SESSION}" -g pane-border-format "#{pane_index} #{pane_title}"

#################################################
# Source project config if exists
#################################################
if [ -f "${ROOT}/.tmux.project.conf" ]; then
    tmux source-file "${ROOT}/.tmux.project.conf"
fi

#################################################
# Select starting window and attach
#################################################
tmux select-window -t "${SESSION}:Kiro"
tmux select-pane -t "${SESSION}:Kiro.0"

echo ""
echo "╔════════════════════════════════════════════════════════════╗"
echo "║         Cortex-M3 Embedded Debug Labs - tmux               ║"
echo "╟────────────────────────────────────────────────────────────╢"
echo "║  Windows:                                                  ║"
echo "║    1. Kiro    - AI assistant + Editor                      ║"
echo "║    2. Debug   - QEMU + GDB + Build                         ║"
echo "║    3. Lab     - Lab workspace                              ║"
echo "║    4. Analysis- objdump + readelf                          ║"
echo "║    5. Git     - Version control                            ║"
echo "╟────────────────────────────────────────────────────────────╢"
echo "║  Quick keys:                                               ║"
echo "║    Prefix + L  - Switch to lab                             ║"
echo "║    Prefix + 1-5- Switch windows                            ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

if [ -n "${LAB}" ]; then
    echo "Starting session with LAB=${LAB}"
fi

tmux attach -t "${SESSION}"
