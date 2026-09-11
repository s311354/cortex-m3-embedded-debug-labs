#!/usr/bin/env bash
# ==============================================================================
# West Build Script for Zephyr Projects
# ==============================================================================
# This script wraps the 'west build' command for building Zephyr applications.
# It provides convenient options for common build workflows.
#
# The script automatically activates the Zephyr Python virtual environment
# located at ~/.venvs/zephyr before running west commands.
#
# Usage:
#   ./scripts/west-build.sh [options]
#   ./scripts/west-build.sh clean          # Clean build directory only
#   ./scripts/west-build.sh --build-dir custom_build clean  # Clean custom dir
#
# Options:
#   -b, --board BOARD        Board target (default: qemu_cortex_m3)
#   -s, --source DIR         Source directory (default: zephyr-header-build)
#   --build-dir DIR          Build output directory (default: build)
#   -p, --pristine           Pristine build (clean before building)
#   -c, --auto-clean         Auto clean build directory before building
#   -f, --flash              Flash after successful build
#   -r, --run                Run with QEMU after successful build
#   -d, --debug              Run with QEMU and wait for debugger
#   -v, --verbose            Verbose build output (also shows contents during clean)
#   -h, --help               Show this help message
#
# Commands:
#   clean                    Remove build directory (build/)
#
# Environment Variables:
#   VENV_PATH                Override virtual environment path
#                            (default: $HOME/.venvs/zephyr)
# ==============================================================================

set -e  # Exit on error

# Default values
WEST_TOPDIR="$(west topdir 2>/dev/null || true)"
BOARD="${BOARD:-qemu_cortex_m3}"
SOURCE_DIR="${SOURCE_DIR:-${WEST_TOPDIR}/zephyr/samples/hello_world}"
BUILD_DIR="build"
VENV_PATH="${VENV_PATH:-${HOME}/.venvs/zephyr}"
COMMAND=""
PRISTINE=false
AUTO_CLEAN=false
FLASH=false
RUN=false
DEBUG=false
VERBOSE=false

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Print functions
print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Show help
show_help() {
    cat << 'EOF'
==============================================================================
West Build Script for Zephyr Projects
==============================================================================
This script wraps the 'west build' command for building Zephyr applications.
It provides convenient options for common build workflows.

The script automatically activates the Zephyr Python virtual environment
located at ~/.venvs/zephyr before running west commands.

Usage:
  ./scripts/west-build.sh [options]
  ./scripts/west-build.sh clean          # Clean build directory only

Options:
  -b, --board BOARD        Board target (default: qemu_cortex_m3)
  -s, --source DIR         Source directory (default: zephyr-header-build)
  --build-dir DIR          Build output directory (default: build)
  -p, --pristine           Pristine build (clean before building)
  -c, --auto-clean         Auto clean build directory before building
  -f, --flash              Flash after successful build
  -r, --run                Run with QEMU after successful build
  -d, --debug              Run with QEMU and wait for debugger
  -v, --verbose            Verbose build output (also shows contents during clean)
  -h, --help               Show this help message

Commands:
  clean                    Remove build directory (build/)

Environment:
  VENV_PATH                Override virtual environment path
                           (default: $HOME/.venvs/zephyr)

Examples:
  ./scripts/west-build.sh                    # Basic build
  ./scripts/west-build.sh clean              # Clean build directory
  ./scripts/west-build.sh -v clean           # Clean with verbose output
  ./scripts/west-build.sh --build-dir custom clean  # Clean custom directory
  ./scripts/west-build.sh -b qemu_cortex_m0  # Different board
  ./scripts/west-build.sh -p -v              # Clean build, verbose
  ./scripts/west-build.sh -r                 # Build and run
  ./scripts/west-build.sh -d                 # Build and debug
==============================================================================
EOF
    exit 0
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        clean)
            COMMAND="clean"
            shift
            ;;
        -b|--board)
            BOARD="$2"
            shift 2
            ;;
        -s|--source)
            SOURCE_DIR="$2"
            shift 2
            ;;
        --build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -p|--pristine)
            PRISTINE=true
            shift
            ;;
        -c|--auto-clean)
            AUTO_CLEAN=true
            shift
            ;;
        -f|--flash)
            FLASH=true
            shift
            ;;
        -r|--run)
            RUN=true
            shift
            ;;
        -d|--debug)
            DEBUG=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            ;;
    esac
done

# Handle clean command
if [[ "$COMMAND" == "clean" ]]; then
    if [[ -d "$BUILD_DIR" ]]; then
        # Show directory size
        DIRSIZE=$(du -sh "$BUILD_DIR" 2>/dev/null | cut -f1)
        print_info "Build directory: $BUILD_DIR (${DIRSIZE:-unknown size})"
        
        # Show what's being deleted in verbose mode
        if [[ "$VERBOSE" == true ]]; then
            print_info "Contents:"
            ls -lh "$BUILD_DIR" 2>/dev/null | head -20
        fi
        
        print_info "Removing build directory..."
        rm -rf "$BUILD_DIR"
        print_success "Build directory cleaned: $BUILD_DIR"
    else
        print_info "Build directory does not exist: $BUILD_DIR"
        print_info "Nothing to clean"
    fi
    exit 0
fi

# Activate Zephyr Python virtual environment
if [[ -f "$VENV_PATH/bin/activate" ]]; then
    print_info "Activating Zephyr virtual environment: $VENV_PATH"
    # Source the virtual environment
    source "$VENV_PATH/bin/activate"
    print_success "Virtual environment activated"
else
    print_warning "Zephyr virtual environment not found at: $VENV_PATH"
    print_info "Attempting to use system-wide west installation..."
fi

# Check if west is installed
if ! command -v west &> /dev/null; then
    print_error "west command not found. Please install Zephyr SDK and west tool."
    print_info "Visit: https://docs.zephyrproject.org/latest/develop/getting_started/index.html"
    exit 1
fi

# Check if source directory exists
if [[ ! -d "$SOURCE_DIR" ]]; then
    print_error "Source directory not found: $SOURCE_DIR"
    exit 1
fi

# Build the west build command
WEST_CMD="west build"

# Add source directory
WEST_CMD="$WEST_CMD -s $SOURCE_DIR"

# Add board
WEST_CMD="$WEST_CMD -b $BOARD"

# Add pristine flag
if [[ "$PRISTINE" == true ]]; then
    WEST_CMD="$WEST_CMD -p always"
elif [[ "$AUTO_CLEAN" == true ]]; then
    WEST_CMD="$WEST_CMD -p auto"
fi

# Add verbose flag
if [[ "$VERBOSE" == true ]]; then
    WEST_CMD="$WEST_CMD -v"
fi

# Set build directory if specified
if [[ -n "$BUILD_DIR" ]]; then
    WEST_CMD="$WEST_CMD -d $BUILD_DIR"
fi

# Print build configuration
echo ""
print_info "=========================================="
print_info "West Build Configuration"
print_info "=========================================="
print_info "Board:          $BOARD"
print_info "Source:         $SOURCE_DIR"
print_info "Build Dir:      $BUILD_DIR"
print_info "Venv:           $VENV_PATH"
print_info "Pristine:       $PRISTINE"
print_info "Auto Clean:     $AUTO_CLEAN"
print_info "Verbose:        $VERBOSE"
print_info "=========================================="
echo ""

# Execute build
print_info "Executing: $WEST_CMD"
echo ""

if $WEST_CMD; then
    print_success "Build completed successfully!"
    
    # Show binary size
    if [[ -f "build/zephyr/zephyr.elf" ]]; then
        print_info "Binary size:"
        arm-none-eabi-size build/zephyr/zephyr.elf || size build/zephyr/zephyr.elf
    fi
    
    # Flash if requested
    if [[ "$FLASH" == true ]]; then
        print_info "Flashing..."
        west flash
    fi
    
    # Run with QEMU if requested
    if [[ "$RUN" == true ]]; then
        print_info "Running with QEMU..."
        west build -t run
    fi
    
    # Debug with QEMU if requested
    if [[ "$DEBUG" == true ]]; then
        print_info "Starting QEMU in debug mode..."
        print_info "Connect with: gdb-multiarch build/zephyr/zephyr.elf -ex 'target remote :1234'"
        west build -t debugserver
    fi
else
    print_error "Build failed!"
    exit 1
fi

echo ""
print_success "Done!"
