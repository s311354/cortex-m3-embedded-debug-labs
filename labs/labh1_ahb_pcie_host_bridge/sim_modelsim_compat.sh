#!/bin/bash
# ModelSim-compatible wrapper for Icarus Verilog
# Usage: source this file to get vlib/vlog/vsim aliases

WORK_LIB="work"

vlib() {
    local lib=${1:-work}
    mkdir -p "$lib"
    echo "Creating library '$lib'"
}

vlog() {
    local files=""
    local work_lib="work"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -work)
                work_lib="$2"
                shift 2
                ;;
            *.v|*.sv)
                files="$files $1"
                shift
                ;;
            *)
                shift
                ;;
        esac
    done
    
    if [ -n "$files" ]; then
        echo "Compiling: $files"
        iverilog -g2012 -o "$work_lib/work.out" $files
    fi
}

vsim() {
    local work_lib="work"
    local top_module=""
    local run_mode="gui"
    
    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -c)
                run_mode="cli"
                shift
                ;;
            -do)
                # Ignore -do for now
                shift 2
                ;;
            *)
                top_module="$1"
                shift
                ;;
        esac
    done
    
    if [ -f "$work_lib/work.out" ]; then
        echo "Running simulation..."
        vvp "$work_lib/work.out"
    else
        echo "Error: No compiled design found in $work_lib"
        return 1
    fi
}

export -f vlib vlog vsim
echo "ModelSim compatibility layer loaded (using Icarus Verilog)"
echo "Available commands: vlib, vlog, vsim"
