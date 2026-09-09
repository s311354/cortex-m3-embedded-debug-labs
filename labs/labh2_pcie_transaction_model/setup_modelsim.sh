#!/bin/bash
#
# Lab: ModelSim Environment Check
# 
# This script validates that all required tools and paths are available
# for running DesignStart simulations with ModelSim in a container.
#

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}/../.."
ARM_M3_ROOT="${PROJECT_ROOT}/../ARM_M3_design"

# Container configuration (defaults)
CONTAINER_ENGINE="${CONTAINER_ENGINE:-podman}"
IMAGE_NAME="${MODELSIM_IMAGE:-localhost/modelsim-runner:local}"
MODELSIM_HOST_PATH="${MODELSIM_HOST_PATH:-/home/shelton/intelFPGA/20.1/modelsim_ase}"
MODELSIM_BIN_PATH="/opt/modelsim/linuxaloem"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_header() {
    echo ""
    echo -e "${BLUE}======================================${NC}"
    echo -e "${BLUE}  ModelSim Environment Check${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo ""
}

print_pass() {
    echo -e "${GREEN}✓${NC} $1"
}

print_fail() {
    echo -e "${RED}✗${NC} $1"
}

print_info() {
    echo -e "${BLUE}ℹ${NC} $1"
}

# Parse command line arguments
SKIP_TEST=0
while [[ $# -gt 0 ]]; do
    case $1 in
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
            echo "  --skip-test          Skip ModelSim functionality test"
            echo "  --image NAME         Specify container image name"
            echo "  --modelsim-path PATH Specify ModelSim installation path"
            echo "  --help, -h           Show this help"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            echo "Run with --help for usage information"
            exit 1
            ;;
    esac
done

print_header

# Check 1: Container engine
if command -v podman &> /dev/null; then
    CONTAINER_ENGINE="podman"
    print_pass "Container engine: Podman"
elif command -v docker &> /dev/null; then
    CONTAINER_ENGINE="docker"
    print_pass "Container engine: Docker"
else
    print_fail "No container engine found (podman or docker required)"
    echo ""
    echo "Install one of:"
    echo "  sudo apt install podman    (recommended)"
    echo "  sudo apt install docker.io"
    exit 1
fi

# Check 2: ModelSim host installation
if [ ! -d "${MODELSIM_HOST_PATH}/linuxaloem" ]; then
    print_fail "ModelSim not found at: ${MODELSIM_HOST_PATH}"
    echo ""
    echo "Set MODELSIM_HOST_PATH to your ModelSim installation:"
    echo "  export MODELSIM_HOST_PATH=/path/to/modelsim"
    exit 1
fi

if [ ! -f "${MODELSIM_HOST_PATH}/linuxaloem/vsim" ]; then
    print_fail "vsim binary not found in ${MODELSIM_HOST_PATH}/linuxaloem"
    exit 1
fi

VSIM_ARCH=$(file "${MODELSIM_HOST_PATH}/linuxaloem/vsim" | grep -oE "(80386|x86-64|x86_64)" || echo "unknown")
print_pass "ModelSim found: ${MODELSIM_HOST_PATH} (${VSIM_ARCH})"

# Check 3: Container image
if ${CONTAINER_ENGINE} image exists "${IMAGE_NAME}" 2>/dev/null; then
    print_pass "Container image: ${IMAGE_NAME}"
else
    print_fail "Container image not found: ${IMAGE_NAME}"
    echo ""
    echo "Available images:"
    ${CONTAINER_ENGINE} images | grep -i modelsim || echo "  (none found)"
    echo ""
    echo "Build or pull the required image first"
    exit 1
fi

# Check 4: ARM DesignStart
if [ ! -d "${ARM_M3_ROOT}/m3designstart" ]; then
    print_fail "ARM DesignStart not found at: ${ARM_M3_ROOT}"
    exit 1
fi
print_pass "ARM DesignStart: ${ARM_M3_ROOT}"

# Check 5: Execution testbench
ARM_EXEC_TB="${ARM_M3_ROOT}/m3designstart/logical/testbench/execution_tb"
if [ ! -d "${ARM_EXEC_TB}" ]; then
    print_fail "Execution testbench not found: ${ARM_EXEC_TB}"
    exit 1
fi
print_pass "Execution testbench: ${ARM_EXEC_TB}"

# Check 6: Test ModelSim functionality (optional)
if [ ${SKIP_TEST} -eq 0 ]; then
    echo ""
    print_info "Testing ModelSim in container..."
    
    if ${CONTAINER_ENGINE} run --rm \
        --platform linux/amd64 \
        -v "${MODELSIM_HOST_PATH}:/opt/modelsim:ro" \
        "${IMAGE_NAME}" \
        ${MODELSIM_BIN_PATH}/vsim -version > /dev/null 2>&1; then
        print_pass "ModelSim functional test passed"
    else
        print_fail "ModelSim test failed"
        echo ""
        echo "Try running with --skip-test to see other checks"
        exit 1
    fi
fi

# Summary
echo ""
echo -e "${BLUE}======================================${NC}"
echo -e "${GREEN}✓ All checks passed!${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""

# Create container wrapper scripts (needed on ARM64 to call x86 ModelSim)
print_info "Creating ModelSim container wrappers..."

WRAPPER_DIR="${SCRIPT_DIR}/modelsim_bin"
mkdir -p "${WRAPPER_DIR}"

for TOOL in vsim vlog vlib vcom vmap vopt; do
    cat > "${WRAPPER_DIR}/${TOOL}" << WRAPPER_EOF
#!/bin/bash
# ModelSim container wrapper for ${TOOL}
# Auto-generated by setup_modelsim.sh
#
# This wrapper runs ModelSim tools inside a container with proper directory mounting
# for both the lab project and ARM DesignStart files.

MODELSIM_HOST_PATH="${MODELSIM_HOST_PATH}"
IMAGE_NAME="${IMAGE_NAME}"
CONTAINER_ENGINE="${CONTAINER_ENGINE}"
WORK_DIR="\$(pwd)"

# Build mount arguments
# ModelSim installation (rw for work library updates)
MOUNT_ARGS="-v \${MODELSIM_HOST_PATH}:/opt/modelsim:rw,Z"

# Auto-mount: parent-parent directory from current working directory
# This covers most cases when running from nested directories
MOUNT_ARGS="\$MOUNT_ARGS -v \$(dirname \$(dirname \$WORK_DIR)):\$(dirname \$(dirname \$WORK_DIR)):rw,Z"

# Add extra mounts if specified (space-separated list of directories)
# Usage: export MODELSIM_EXTRA_MOUNTS="/path/to/dir1 /path/to/dir2"
# This is CRITICAL for DesignStart integration - must mount ARM_M3_design
if [ -n "\$MODELSIM_EXTRA_MOUNTS" ]; then
    for mount in \$MODELSIM_EXTRA_MOUNTS; do
        if [ -d "\$mount" ]; then
            MOUNT_ARGS="\$MOUNT_ARGS -v \$mount:\$mount:rw,Z"
        fi
    done
fi

exec \${CONTAINER_ENGINE} run --rm \\
    --platform linux/amd64 \\
    \$MOUNT_ARGS \\
    -w "\${WORK_DIR}" \\
    --userns=keep-id \\
    -e PATH="${MODELSIM_BIN_PATH}:/usr/local/bin:/usr/bin:/bin" \\
    -e HOME="\${HOME}" \\
    -e USER="\${USER}" \\
    -e LM_LICENSE_FILE="\${LM_LICENSE_FILE:-@localhost}" \\
    -e MODEL_TECH=/opt/modelsim/modeltech \\
    -e MTI_VCO_MODE="\${MTI_VCO_MODE:-}" \\
    "\${IMAGE_NAME}" \\
    ${MODELSIM_BIN_PATH}/${TOOL} "\$@"
WRAPPER_EOF
    
    chmod +x "${WRAPPER_DIR}/${TOOL}"
done

print_pass "Wrappers created in ${WRAPPER_DIR}"

echo ""
echo "Configuration:"
echo "  Container:      ${CONTAINER_ENGINE}"
echo "  Image:          ${IMAGE_NAME}"
echo "  ModelSim path:  ${MODELSIM_HOST_PATH}"
echo "  DesignStart:    ${ARM_M3_ROOT}"
echo "  Wrappers:       ${WRAPPER_DIR}"
echo ""
echo -e "${BLUE}======================================${NC}"
echo -e "${BLUE}Next Steps${NC}"
echo -e "${BLUE}======================================${NC}"
echo ""
echo "1. Add wrappers to PATH:"
echo "   export PATH=\"${WRAPPER_DIR}:\$PATH\""
echo ""
echo ""
echo "2. Build and run:"
echo "   make designstart-compile # Just compile RTL"
echo "   make designstart-run     # Run existing simulation"
echo "   make designstart-all     # Full clean build + simulation"
echo ""
echo -e "${GREEN}Note:${NC} The designstart.mk makefile automatically sets"
echo "      MODELSIM_EXTRA_MOUNTS for proper directory access."
echo ""
