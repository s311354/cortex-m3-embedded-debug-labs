#!/usr/bin/env bash
#
# Lab Switcher - Quick navigation between labs in tmux
#

set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LAB="$1"

if [ -z "${LAB}" ]; then
    echo "Available labs:"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    for lab_dir in "${ROOT}"/labs/lab*; do
        if [ -d "${lab_dir}" ]; then
            lab_name=$(basename "${lab_dir}")
            lab_num=$(echo "${lab_name}" | grep -oP 'lab\K\d+')
            
            # Get first line of main.c as description if available
            main_file="${lab_dir}/main.c"
            if [ -f "${main_file}" ]; then
                desc=$(head -n 5 "${main_file}" | grep -oP '//\s*\K.*' | head -n 1)
            else
                desc="See lab directory"
            fi
            
            printf "  %-30s %s\n" "${lab_name}" "${desc:-}"
        fi
    done
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo ""
    echo "Usage: $0 <lab_name>"
    echo "Example: $0 lab01_core_registers"
    echo ""
    echo "Or with number: $0 01"
    exit 1
fi

# Handle numeric input (e.g., "01" -> "lab01_*")
if [[ "${LAB}" =~ ^[0-9]+$ ]]; then
    LAB_NUM=$(printf "%02d" "$((10#${LAB}))")
    LAB_MATCH=$(find "${ROOT}/labs" -maxdepth 1 -type d -name "lab${LAB_NUM}_*" | head -n 1)
    if [ -n "${LAB_MATCH}" ]; then
        LAB=$(basename "${LAB_MATCH}")
    else
        echo "Error: Lab ${LAB_NUM} not found"
        exit 1
    fi
fi

LAB_PATH="${ROOT}/labs/${LAB}"

if [ ! -d "${LAB_PATH}" ]; then
    echo "Error: Lab directory '${LAB}' not found at ${LAB_PATH}"
    exit 1
fi

echo "Switching to ${LAB}..."

# Check if we're in a tmux session
if [ -n "${TMUX}" ]; then
    # Send commands to Lab window panes
    tmux send-keys -t Lab.0 "cd ${LAB_PATH}" C-m
    tmux send-keys -t Lab.0 "clear && ls -la" C-m
    
    tmux send-keys -t Lab.1 "clear && echo '# Ready to build ${LAB}'" C-m
    
    # Update Debug window with lab-specific commands
    tmux send-keys -t Debug.0 "# make debug LAB=${LAB}" C-m
    tmux send-keys -t Debug.1 "# make gdb LAB=${LAB}" C-m
    tmux send-keys -t Debug.2 "make clean" C-m
    
    # Switch to Lab window
    tmux select-window -t Lab
    
    echo "Switched to ${LAB}"
    echo ""
    echo "Next steps:"
    echo "  1. Window 'Lab'     - Edit and build the lab"
    echo "  2. Window 'Debug'   - Run 'make debug LAB=${LAB}' in pane 0"
    echo "  3. Window 'Debug'   - Run 'make gdb LAB=${LAB}' in pane 1"
    echo "  4. Window 'Kiro'    - Ask Kiro for help with the lab"
else
    # Not in tmux, just cd to the lab
    cd "${LAB_PATH}"
    echo "Changed directory to ${LAB_PATH}"
    echo "To use tmux integration, run: ${ROOT}/scripts/tmux.sh"
fi
