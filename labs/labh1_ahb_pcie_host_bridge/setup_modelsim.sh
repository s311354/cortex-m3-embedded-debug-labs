#!/bin/bash
#
# Lab H1: Setup ModelSim with Podman/Docker for DesignStart simulation
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."
ARM_M3_ROOT="${PROJECT_ROOT}/../ARM_M3_design"

# Container configuration
CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"
IMAGE_NAME="${MODELSIM_IMAGE:-localhost/modelsim-runner:local}"
WORK_DIR="/workspace"

# ModelSim host installation paths
MODELSIM_HOST_PATH="${MODELSIM_HOST_PATH:-/home/shelton/intelFPGA/20.1/modelsim_ase}"
MODELSIM_CONTAINER_PATH="/opt/modelsim"
MODELSIM_BIN_PATH="${MODELSIM_CONTAINER_PATH}/linuxaloem"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_banner() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}  ModelSim Container Setup${NC}"
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

check_container_engine() {
    print_step "Checking container engine..."
    
    if command -v podman &> /dev/null; then
        CONTAINER_ENGINE="podman"
        print_info "Using Podman"
    elif command -v docker &> /dev/null; then
        CONTAINER_ENGINE="docker"
        print_info "Using Docker"
    else
        print_error "Neither Podman nor Docker found"
        echo "Please install one of them:"
        echo "  - Podman: sudo apt install podman (recommended)"
        echo "  - Docker: sudo apt install docker.io"
        exit 1
    fi
}

check_image() {
    print_step "Checking ModelSim image..."
    
    if ${CONTAINER_ENGINE} image exists "${IMAGE_NAME}" 2>/dev/null; then
        print_info "Image found: ${IMAGE_NAME}"
        return 0
    else
        print_warning "Image not found: ${IMAGE_NAME}"
        return 1
    fi
}

check_modelsim_host() {
    print_step "Checking ModelSim host installation..."
    
    if [ ! -d "${MODELSIM_HOST_PATH}" ]; then
        print_error "ModelSim not found at: ${MODELSIM_HOST_PATH}"
        echo ""
        echo "Please install ModelSim or set MODELSIM_HOST_PATH environment variable"
        echo "Expected path: ${MODELSIM_HOST_PATH}"
        exit 1
    fi
    
    if [ ! -d "${MODELSIM_HOST_PATH}/linuxaloem" ]; then
        print_error "ModelSim binaries not found at: ${MODELSIM_HOST_PATH}/linuxaloem"
        exit 1
    fi
    
    if [ ! -f "${MODELSIM_HOST_PATH}/linuxaloem/vsim" ]; then
        print_error "vsim binary not found"
        exit 1
    fi
    
    print_info "Found ModelSim at: ${MODELSIM_HOST_PATH}"
    
    # Check architecture
    VSIM_ARCH=$(file "${MODELSIM_HOST_PATH}/linuxaloem/vsim" | grep -oE "(80386|x86-64|x86_64)")
    print_info "ModelSim architecture: ${VSIM_ARCH}"
}

setup_designstart_libs() {
    print_step "Setting up DesignStart libraries..."
    
    if [ ! -d "${ARM_M3_ROOT}" ]; then
        print_error "ARM DesignStart not found at: ${ARM_M3_ROOT}"
        exit 1
    fi
    
    ARM_EXEC_TB="${ARM_M3_ROOT}/m3designstart/logical/testbench/execution_tb"
    
    if [ ! -d "${ARM_EXEC_TB}" ]; then
        print_error "Execution testbench not found"
        exit 1
    fi
    
    print_info "Compiling DesignStart baseline libraries..."
    
    make -C "${ARM_EXEC_TB}" clean
    make -C "${ARM_EXEC_TB}" compile SIMULATOR=mti
    
    print_step "Libraries compiled successfully"
}

create_wrapper_script() {
    print_step "Creating ModelSim wrapper scripts..."
    
    WRAPPER_DIR="${SCRIPT_DIR}/modelsim_bin"
    mkdir -p "${WRAPPER_DIR}"
    
    # Create main wrapper
    MAIN_WRAPPER="${SCRIPT_DIR}/modelsim_wrapper.sh"
    
    cat > "${MAIN_WRAPPER}" << 'EOF'
#!/bin/bash
#
# ModelSim wrapper script for containerized execution
#

CONTAINER_ENGINE="podman"
IMAGE_NAME="localhost/modelsim-runner:local"
MODELSIM_HOST_PATH="/home/shelton/intelFPGA/20.1/modelsim_ase"
MODELSIM_CONTAINER_PATH="/opt/modelsim"
MODELSIM_BIN_PATH="/opt/modelsim/linuxaloem"

# Mount current directory and ARM DesignStart
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ARM_M3_ROOT="$(realpath "${PROJECT_ROOT}/../ARM_M3_design")"

# Determine working directory - use absolute path in container
WORK_DIR="$(realpath "${PWD}")"

# Get the command name (vsim, vlog, vcom, etc.)
if [ $# -gt 0 ]; then
    CMD="$@"
else
    CMD="/bin/bash"
fi

# Run in container with ModelSim and workspace mounted
${CONTAINER_ENGINE} run --rm -it \
    --platform linux/amd64 \
    -v "${MODELSIM_HOST_PATH}:${MODELSIM_CONTAINER_PATH}:ro" \
    -v "${ARM_M3_ROOT}:${ARM_M3_ROOT}" \
    -w "${WORK_DIR}" \
    --userns=keep-id \
    -e PATH="${MODELSIM_BIN_PATH}:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    "${IMAGE_NAME}" \
    ${CMD}
EOF
    
    chmod +x "${MAIN_WRAPPER}"
    print_info "Main wrapper created: ${MAIN_WRAPPER}"
    
    # Create individual tool wrappers (vsim, vlog, vcom, vlib, vmap, etc.)
    for TOOL in vsim vlog vcom vlib vmap vdel vopt; do
        TOOL_WRAPPER="${WRAPPER_DIR}/${TOOL}"
        
        cat > "${TOOL_WRAPPER}" << 'EOF'
#!/bin/bash
CONTAINER_ENGINE="podman"
IMAGE_NAME="localhost/modelsim-runner:local"
MODELSIM_HOST_PATH="/home/shelton/intelFPGA/20.1/modelsim_ase"
MODELSIM_CONTAINER_PATH="/opt/modelsim"
MODELSIM_BIN_PATH="/opt/modelsim/linuxaloem"

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
ARM_M3_ROOT="$(realpath "${PROJECT_ROOT}/../ARM_M3_design")"
LABS_ROOT="$(realpath "${PROJECT_ROOT}")"

# Determine working directory - use absolute path in container
WORK_DIR="$(realpath "${PWD}")"

${CONTAINER_ENGINE} run --rm \
    --platform linux/amd64 \
    -v "${MODELSIM_HOST_PATH}:${MODELSIM_CONTAINER_PATH}:ro" \
    -v "${ARM_M3_ROOT}:${ARM_M3_ROOT}" \
    -v "${LABS_ROOT}:${LABS_ROOT}" \
    -w "${WORK_DIR}" \
    --userns=keep-id \
    -e PATH="${MODELSIM_BIN_PATH}:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
    "${IMAGE_NAME}" \
    TOOL_PLACEHOLDER "$@"
EOF
        
        # Replace the tool placeholder with actual tool name
        sed -i "s/TOOL_PLACEHOLDER/${TOOL}/" "${TOOL_WRAPPER}"
        
        chmod +x "${TOOL_WRAPPER}"
    done
    
    print_info "Tool wrappers created in: ${WRAPPER_DIR}"
    print_info "Available tools: vsim, vlog, vcom, vlib, vmap, vdel, vopt"
}

test_modelsim() {
    print_step "Testing ModelSim in container..."
    
    ${CONTAINER_ENGINE} run --rm \
        --platform linux/amd64 \
        -v "${MODELSIM_HOST_PATH}:${MODELSIM_CONTAINER_PATH}:ro" \
        -v "${PWD}:/work" \
        -w /work \
        "${IMAGE_NAME}" \
        ${MODELSIM_BIN_PATH}/vsim -version
    
    if [ $? -eq 0 ]; then
        print_step "ModelSim is working correctly in container"
    else
        print_error "ModelSim test failed"
        exit 1
    fi
}

configure_makefile() {
    print_step "Configuring Makefile for container usage..."
    
    # Create environment configuration file
    ENV_FILE="${SCRIPT_DIR}/modelsim_env.sh"
    
    cat > "${ENV_FILE}" << EOF
#!/bin/bash
# ModelSim container environment configuration
# Source this file before running make commands

export MODELSIM_CONTAINER="${CONTAINER_ENGINE}"
export MODELSIM_IMAGE="${IMAGE_NAME}"
export MODELSIM_HOST_PATH="${MODELSIM_HOST_PATH}"
export MODELSIM_CONTAINER_PATH="${MODELSIM_CONTAINER_PATH}"
export MODELSIM_BIN_PATH="${MODELSIM_BIN_PATH}"

# Add tool wrappers to PATH
export PATH="${SCRIPT_DIR}/modelsim_bin:\${PATH}"

echo "ModelSim container environment configured"
echo "  Container: \${MODELSIM_CONTAINER}"
echo "  Image: \${MODELSIM_IMAGE}"
echo "  Host path: \${MODELSIM_HOST_PATH}"
echo "  Tools in PATH: ${SCRIPT_DIR}/modelsim_bin"
EOF
    
    chmod +x "${ENV_FILE}"
    print_info "Environment file created: ${ENV_FILE}"
    
    # Create Makefile include
    if [ -f "${SCRIPT_DIR}/Makefile.modelsim" ]; then
        print_info "Makefile.modelsim already exists"
    else
        cat > "${SCRIPT_DIR}/Makefile.modelsim" << EOF
# ModelSim container configuration
# Include this in your Makefile with: include Makefile.modelsim

MODELSIM_CONTAINER := ${CONTAINER_ENGINE}
MODELSIM_IMAGE := ${IMAGE_NAME}
MODELSIM_HOST_PATH := ${MODELSIM_HOST_PATH}
MODELSIM_CONTAINER_PATH := ${MODELSIM_CONTAINER_PATH}
MODELSIM_BIN_PATH := ${MODELSIM_BIN_PATH}
MODELSIM_WRAPPER_DIR := ${SCRIPT_DIR}/modelsim_bin

# Container run command template
MODELSIM_RUN = \$(MODELSIM_CONTAINER) run --rm \\
    --platform linux/amd64 \\
    -v "\$(MODELSIM_HOST_PATH):\$(MODELSIM_CONTAINER_PATH):ro" \\
    -v "\$(PWD):/work" \\
    -w /work \\
    --userns=keep-id \\
    -e PATH="\$(MODELSIM_BIN_PATH):/usr/local/bin:/usr/bin:/bin" \\
    "\$(MODELSIM_IMAGE)"

# Tool definitions
VLOG := \$(MODELSIM_RUN) vlog
VCOM := \$(MODELSIM_RUN) vcom
VSIM := \$(MODELSIM_RUN) vsim
VLIB := \$(MODELSIM_RUN) vlib
VMAP := \$(MODELSIM_RUN) vmap

# Or use wrapper directory
export PATH := \$(MODELSIM_WRAPPER_DIR):\$(PATH)
EOF
        print_info "Created Makefile.modelsim"
    fi
}

show_usage_info() {
    echo ""
    echo -e "${BLUE}============================================${NC}"
    echo -e "${GREEN}✓ Setup Complete!${NC}"
    echo -e "${BLUE}============================================${NC}"
    echo ""
    echo "ModelSim Configuration:"
    echo "  Host path:      ${MODELSIM_HOST_PATH}"
    echo "  Container path: ${MODELSIM_CONTAINER_PATH}"
    echo "  Binary path:    ${MODELSIM_BIN_PATH}"
    echo "  Container:      ${CONTAINER_ENGINE}"
    echo "  Image:          ${IMAGE_NAME}"
    echo ""
    echo "Next steps:"
    echo ""
    echo "  1. Source the environment and use Make targets directly (for current shell):"
    echo "     source ./modelsim_env.sh && make designstart-compile"
    echo "     source ./modelsim_env.sh && make designstart-run"
    echo ""
    echo "  2. Run DesignStart simulation:"
    echo "     ./run_designstart.sh"
    echo ""
}

main() {
    print_banner
    
    # Parse options
    SKIP_LIBS=0
    SKIP_TEST=0
    
    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-libs)
                SKIP_LIBS=1
                shift
                ;;
            --skip-test)
                SKIP_TEST=1
                shift
                ;;
            --image)
                IMAGE_NAME="$2"
                shift 2
                ;;
            --modelsim-path)
                MODELSIM_HOST_PATH="$2"
                shift 2
                ;;
            --help|-h)
                echo "Usage: $0 [OPTIONS]"
                echo ""
                echo "Options:"
                echo "  --skip-libs          Skip DesignStart library compilation"
                echo "  --skip-test          Skip ModelSim test"
                echo "  --image NAME         Specify container image (default: localhost/modelsim-runner:local)"
                echo "  --modelsim-path PATH Specify ModelSim host path (default: /home/shelton/intelFPGA/20.1/modelsim_ase)"
                echo "  --help, -h           Show this help"
                exit 0
                ;;
            *)
                print_error "Unknown option: $1"
                exit 1
                ;;
        esac
    done
    
    check_container_engine
    check_modelsim_host
    
    if ! check_image; then
        print_error "Container image not available: ${IMAGE_NAME}"
        echo ""
        echo "Please ensure the container image exists:"
        echo "  ${CONTAINER_ENGINE} images | grep modelsim"
        echo ""
        echo "Available images:"
        ${CONTAINER_ENGINE} images | head -5
        exit 1
    fi
    
    if [ ${SKIP_TEST} -eq 0 ]; then
        test_modelsim
    fi
    
    create_wrapper_script
    configure_makefile
    
    if [ ${SKIP_LIBS} -eq 0 ]; then
        setup_designstart_libs
    else
        print_warning "Skipping library setup"
        print_warning "You'll need to compile DesignStart libraries manually"
    fi
    
    show_usage_info
}

main "$@"
