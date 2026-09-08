#!/bin/bash
#
# Lab H1: Run ARM DesignStart full-system simulation
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LAB_ROOT="${SCRIPT_DIR}"
PROJECT_ROOT="${LAB_ROOT}/../.."

ARM_M3_ROOT="${PROJECT_ROOT}/../ARM_M3_design"
ARM_EXEC_TB="${ARM_M3_ROOT}/m3designstart/logical/testbench/execution_tb"

# Simulation configuration
SIMULATOR="${ARM_SIM:-mti}"
TESTNAME="pcie_host_smoke"
BUILD_DIR="${LAB_ROOT}/build"
LOG_DIR="${BUILD_DIR}/logs"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

print_banner() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}  Lab H1: DesignStart Simulation${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
}

print_step() {
    echo -e "${GREEN}[*]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

check_environment() {
    print_step "Checking environment..."
    
    if [ ! -d "${ARM_M3_ROOT}" ]; then
        print_error "ARM DesignStart not found at: ${ARM_M3_ROOT}"
        echo "Please set ARM_M3_ROOT environment variable or place DesignStart at expected location"
        exit 1
    fi
    
    if [ ! -d "${ARM_EXEC_TB}" ]; then
        print_error "Execution testbench not found at: ${ARM_EXEC_TB}"
        exit 1
    fi
    
    # Check for ModelSim setup
    if [ -f "${LAB_ROOT}/modelsim_env.sh" ]; then
        print_step "Loading ModelSim environment..."
        source "${LAB_ROOT}/modelsim_env.sh"
    else
        print_warning "ModelSim environment not configured"
        print_warning "Run './setup_modelsim.sh' first to configure ModelSim container"
        
        # Check if vsim is available natively
        if ! command -v vsim &> /dev/null; then
            print_error "ModelSim not found in PATH and container not configured"
            echo ""
            echo "Please run: ./setup_modelsim.sh"
            exit 1
        else
            print_info "Using native ModelSim from PATH"
        fi
    fi
    
    print_step "Environment OK"
}

create_directories() {
    mkdir -p "${BUILD_DIR}"
    mkdir -p "${LOG_DIR}"
}

build_labh1() {
    print_step "Building Lab H1 firmware..."
    
    cd "${LAB_ROOT}"
    make clean
    make
    
    if [ ! -f "${LAB_ROOT}/labh1_ahb_pcie_host_bridge.elf" ]; then
        print_error "Firmware build failed - ELF not found"
        echo "Expected: ${LAB_ROOT}/labh1_ahb_pcie_host_bridge.elf"
        exit 1
    fi
    
    print_step "Firmware build complete"
}

compile_rtl() {
    print_step "Compiling DesignStart + Lab H1 RTL..."
    
    # Ensure ModelSim wrappers are in PATH
    export PATH="${LAB_ROOT}/modelsim_bin:${PATH}"
    
    # Use 32-bit mode (ModelSim Starter Edition is 32-bit only)
    make -C "${LAB_ROOT}" designstart-compile \
        SIMULATOR="${SIMULATOR}" \
        SIM_64BIT=no
    
    print_step "RTL compilation complete"
}

prepare_testcode() {
    print_step "Preparing test code..."
    
    cd "${LAB_ROOT}"
    
    # Copy firmware to DesignStart testcode directory
    TESTCODE_DIR="${ARM_EXEC_TB}/testcodes/${TESTNAME}"
    mkdir -p "${TESTCODE_DIR}"
    
    if [ -f "${LAB_ROOT}/labh1_ahb_pcie_host_bridge.hex" ]; then
        cp "${LAB_ROOT}/labh1_ahb_pcie_host_bridge.hex" "${TESTCODE_DIR}/image.hex"
        print_info "Using HEX file: ${LAB_ROOT}/labh1_ahb_pcie_host_bridge.hex"
    elif [ -f "${LAB_ROOT}/labh1_ahb_pcie_host_bridge.elf" ]; then
        cp "${LAB_ROOT}/labh1_ahb_pcie_host_bridge.elf" "${TESTCODE_DIR}/image.elf"
        print_info "Using ELF file: ${LAB_ROOT}/labh1_ahb_pcie_host_bridge.elf"
    else
        print_error "Neither HEX nor ELF file found in ${LAB_ROOT}"
        exit 1
    fi
    
    print_step "Test code ready at: ${TESTCODE_DIR}"
}

run_simulation() {
    print_step "Starting simulation..."
    
    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    LOG_FILE="${LOG_DIR}/simulation_${TIMESTAMP}.log"
    
    echo "Simulation log: ${LOG_FILE}"
    echo ""
    
    # Ensure ModelSim wrappers are in PATH for DesignStart make
    export PATH="${LAB_ROOT}/modelsim_bin:${PATH}"
    
    print_info "Using vsim: $(which vsim)"
    
    # Use 32-bit mode (ModelSim Starter Edition is 32-bit only)
    make -C "${ARM_EXEC_TB}" run \
        TESTNAME="${TESTNAME}" \
        SIMULATOR="${SIMULATOR}" \
        SIM_64BIT=no \
        2>&1 | tee "${LOG_FILE}"
    
    RESULT=${PIPESTATUS[0]}
    
    if [ ${RESULT} -eq 0 ]; then
        print_step "Simulation completed"
    else
        print_error "Simulation failed with exit code ${RESULT}"
        exit ${RESULT}
    fi
}

main() {
    print_banner
    
    check_environment
    create_directories
    
    # Parse command line options
    SKIP_BUILD=0
    SKIP_COMPILE=0
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-build)
                SKIP_BUILD=1
                shift
                ;;
            --skip-compile)
                SKIP_COMPILE=1
                shift
                ;;
            --sim)
                SIMULATOR="$2"
                shift 2
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --skip-build      Skip firmware build"
                echo "  --skip-compile    Skip RTL compilation"
                echo "  --sim SIMULATOR   Specify simulator (default: mti)"
                echo "  --help, -h        Show this help"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    if [ ${SKIP_BUILD} -eq 0 ]; then
        build_labh1
    else
        print_warning "Skipping firmware build"
    fi
    
    if [ ${SKIP_COMPILE} -eq 0 ]; then
        compile_rtl
    else
        print_warning "Skipping RTL compilation"
    fi
    
    prepare_testcode
    run_simulation
}

main "$@"
