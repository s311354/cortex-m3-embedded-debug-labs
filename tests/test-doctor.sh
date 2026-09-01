#!/bin/sh
#
# tests/test-doctor.sh
#
# Unit tests for scripts/doctor.sh
#
# Tests environment validation including:
# - ARM toolchain presence (gcc, objcopy, objdump, size)
# - QEMU emulator availability
# - GDB debugger availability
# - Verilog toolchain (iverilog, vvp, verilator, gtkwave)
# - ARM_M3_design directory structure
# - Custom CORTEX_ROOT support
#

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
DOCTOR="${ROOT}/scripts/doctor.sh"

PASS=0
FAIL=0

#########################################
# Test Assertion Helpers
#########################################

pass()
{
	PASS=$((PASS + 1))
	printf "[PASS] %s\n" "$1"
}

fail()
{
	FAIL=$((FAIL + 1))
	printf "[FAIL] %s\n" "$1"
}

# Verify exit status matches expected value
expect_exit()
{
	expected="$1"
	actual="$2"
	description="$3"

	if [ "$expected" -eq "$actual" ]; then
		pass "$description"
	else
		fail "$description"
		printf "  expected=%s actual=%s\n" "$expected" "$actual"
	fi
}

# Verify output contains expected text
expect_contains()
{
	output="$1"
	text="$2"
	description="$3"

	case "$output" in
		*"$text"*)
			pass "$description"
			;;
		*)
			fail "$description"
			printf "Expected output to contain:\n%s\n" "$text"
			printf "\nActual output:\n%s\n" "$output"
			;;
	esac
}

#########################################
# Mock Toolchain Creation
#########################################

# Create a minimal mock executable in the specified directory
make_tool()
{
	dir="$1"
	tool="$2"

	cat > "${dir}/${tool}" << 'EOF'
#!/bin/sh
exit 0
EOF
	chmod +x "${dir}/${tool}"
}

# Create complete mock toolchain with all required tools
create_toolchain()
{
	dir="$1"

	make_tool "$dir" arm-none-eabi-gcc
	make_tool "$dir" arm-none-eabi-objcopy
	make_tool "$dir" arm-none-eabi-objdump
	make_tool "$dir" arm-none-eabi-size
	make_tool "$dir" qemu-system-arm
	make_tool "$dir" gdb-multiarch
	make_tool "$dir" iverilog
	make_tool "$dir" vvp
	make_tool "$dir" verilator
	make_tool "$dir" gtkwave
}

#########################################
# Mock ARM_M3_design Directory Structure
#########################################

# Create minimal ARM_M3_design directory structure with CMSIS headers
create_cortex_root()
{
	root="$1"

	# Create CMSIS directory structure
	mkdir -p \
		"$root/m3designstart/software/cmsis/CMSIS/Include"

	mkdir -p \
		"$root/m3designstart/software/cmsis/Device/ARM/CM3DS/Include"

	# Create required header files
	touch \
		"$root/m3designstart/software/cmsis/CMSIS/Include/core_cm3.h"

	touch \
		"$root/m3designstart/software/cmsis/Device/ARM/CM3DS/Include/CM3DS_MPS2.h"
}


#########################################
# Test Execution Helper
#########################################

# Run doctor.sh with custom PATH and CORTEX_ROOT
run_doctor()
{
	PATH="$1" \
	CORTEX_ROOT="$2" \
	sh "$DOCTOR" > "$TMP/output.txt" 2>&1

	return $?
}

# Run doctor.sh with isolated PATH (for testing missing tools)
# Creates minimal shell environment without development tools
run_doctor_isolated()
{
	custom_path="$1"
	cortex_root="$2"
	output_file="$3"
	
	# Create minimal shell utilities directory
	shell_dir="$custom_path/shell"
	mkdir -p "$shell_dir"
	
	# Symlink only essential shell utilities (not development tools)
	for util in sh dash printf cat grep sed awk dirname basename pwd mkdir rm test; do
		if [ -f "/usr/bin/$util" ]; then
			ln -sf "/usr/bin/$util" "$shell_dir/$util"
		fi
	done
	
	# Symlink [ and command which are special
	ln -sf "/usr/bin/[" "$shell_dir/["
	if [ -f "/usr/bin/command" ]; then
		ln -sf "/usr/bin/command" "$shell_dir/command"
	fi
	
	PATH="$custom_path:$shell_dir" \
	CORTEX_ROOT="$cortex_root" \
	sh "$DOCTOR" > "$output_file" 2>&1
	
	return $?
}

#########################################
# Test 1: Complete Valid Environment
#########################################

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

BIN="$TMP/bin"
ROOTDIR="$TMP/ARM_M3_design"

mkdir -p "$BIN"

create_toolchain "$BIN"
create_cortex_root "$ROOTDIR"

set +e
run_doctor "$BIN:/usr/bin:/bin" "$ROOTDIR"
STATUS=$?
set -e

OUTPUT=$(cat "$TMP/output.txt")

expect_exit 0 "$STATUS" \
	"doctor succeeds with complete environment"

expect_contains "$OUTPUT" \
	"Environment is ready" \
	"environment ready message"

#########################################
# Test 2: Missing GCC Compiler
#########################################

TMP2=$(mktemp -d)

BIN="$TMP2/bin"
ROOTDIR="$TMP2/ARM_M3_design"

mkdir -p "$BIN"

# Create all tools except gcc
make_tool "$BIN" arm-none-eabi-objcopy
make_tool "$BIN" arm-none-eabi-objdump
make_tool "$BIN" arm-none-eabi-size
make_tool "$BIN" qemu-system-arm
make_tool "$BIN" gdb-multiarch

create_cortex_root "$ROOTDIR"

set +e

PATH="$BIN:/usr/bin:/bin" \
CORTEX_ROOT="$ROOTDIR" \
sh "$DOCTOR" > "$TMP2/output.txt" 2>&1

STATUS=$?

set -e

OUTPUT=$(cat "$TMP2/output.txt")

expect_exit 1 "$STATUS" \
	"missing gcc returns failure"

expect_contains "$OUTPUT" \
	"arm-none-eabi-gcc" \
	"missing gcc detected"

rm -rf "$TMP2"

#########################################
# Test 3: Missing ARM_M3_design
#########################################

TMP3=$(mktemp -d)

BIN="$TMP3/bin"

mkdir -p "$BIN"

create_toolchain "$BIN"

set +e

PATH="$BIN:/usr/bin:/bin" \
CORTEX_ROOT="$TMP3/missing" \
sh "$DOCTOR" > "$TMP3/output.txt" 2>&1

STATUS=$?

set -e

OUTPUT=$(cat "$TMP3/output.txt")

expect_exit 1 "$STATUS" \
	"missing ARM_M3_design returns failure"

expect_contains "$OUTPUT" \
	"ARM_M3_design" \
	"missing cortex root detected"

expect_contains "$OUTPUT" \
	"CORTEX_ROOT" \
	"custom CORTEX_ROOT hint"

rm -rf "$TMP3"

#########################################
# Test 4: Custom CORTEX_ROOT
#########################################

TMP4=$(mktemp -d)

BIN="$TMP4/bin"
CUSTOM="$TMP4/custom"

mkdir -p "$BIN"

create_toolchain "$BIN"
create_cortex_root "$CUSTOM"

set +e

PATH="$BIN:/usr/bin:/bin" \
CORTEX_ROOT="$CUSTOM" \
sh "$DOCTOR" > "$TMP4/output.txt" 2>&1

STATUS=$?

set -e

OUTPUT=$(cat "$TMP4/output.txt")

expect_exit 0 "$STATUS" \
	"custom CORTEX_ROOT accepted"

expect_contains "$OUTPUT" \
	"$CUSTOM" \
	"custom path printed"

rm -rf "$TMP4"

#########################################
# Test 5: Missing Verilog Tools (iverilog)
#########################################

TMP5=$(mktemp -d)

BIN="$TMP5/bin"
ROOTDIR="$TMP5/ARM_M3_design"

mkdir -p "$BIN"

# Create all tools except iverilog
make_tool "$BIN" arm-none-eabi-gcc
make_tool "$BIN" arm-none-eabi-objcopy
make_tool "$BIN" arm-none-eabi-objdump
make_tool "$BIN" arm-none-eabi-size
make_tool "$BIN" qemu-system-arm
make_tool "$BIN" gdb-multiarch
make_tool "$BIN" vvp
make_tool "$BIN" verilator
make_tool "$BIN" gtkwave

create_cortex_root "$ROOTDIR"

set +e
run_doctor_isolated "$BIN" "$ROOTDIR" "$TMP5/output.txt"
STATUS=$?
set -e

OUTPUT=$(cat "$TMP5/output.txt")

expect_exit 1 "$STATUS" \
	"missing iverilog returns failure"

expect_contains "$OUTPUT" \
	"iverilog" \
	"missing iverilog detected"

expect_contains "$OUTPUT" \
	"Icarus Verilog" \
	"iverilog install hint"

rm -rf "$TMP5"

#########################################
# Test 6: Missing Verilator
#########################################

TMP6=$(mktemp -d)

BIN="$TMP6/bin"
ROOTDIR="$TMP6/ARM_M3_design"

mkdir -p "$BIN"

# Create all tools except verilator
make_tool "$BIN" arm-none-eabi-gcc
make_tool "$BIN" arm-none-eabi-objcopy
make_tool "$BIN" arm-none-eabi-objdump
make_tool "$BIN" arm-none-eabi-size
make_tool "$BIN" qemu-system-arm
make_tool "$BIN" gdb-multiarch
make_tool "$BIN" iverilog
make_tool "$BIN" vvp
make_tool "$BIN" gtkwave

create_cortex_root "$ROOTDIR"

set +e
run_doctor_isolated "$BIN" "$ROOTDIR" "$TMP6/output.txt"
STATUS=$?
set -e

OUTPUT=$(cat "$TMP6/output.txt")

expect_exit 1 "$STATUS" \
	"missing verilator returns failure"

expect_contains "$OUTPUT" \
	"verilator" \
	"missing verilator detected"

expect_contains "$OUTPUT" \
	"Verilator" \
	"verilator install hint"

rm -rf "$TMP6"

#########################################
# Test 7: Missing GTKWave
#########################################

TMP7=$(mktemp -d)

BIN="$TMP7/bin"
ROOTDIR="$TMP7/ARM_M3_design"

mkdir -p "$BIN"

# Create all tools except gtkwave
make_tool "$BIN" arm-none-eabi-gcc
make_tool "$BIN" arm-none-eabi-objcopy
make_tool "$BIN" arm-none-eabi-objdump
make_tool "$BIN" arm-none-eabi-size
make_tool "$BIN" qemu-system-arm
make_tool "$BIN" gdb-multiarch
make_tool "$BIN" iverilog
make_tool "$BIN" vvp
make_tool "$BIN" verilator

create_cortex_root "$ROOTDIR"

set +e
run_doctor_isolated "$BIN" "$ROOTDIR" "$TMP7/output.txt"
STATUS=$?
set -e

OUTPUT=$(cat "$TMP7/output.txt")

expect_exit 1 "$STATUS" \
	"missing gtkwave returns failure"

expect_contains "$OUTPUT" \
	"gtkwave" \
	"missing gtkwave detected"

expect_contains "$OUTPUT" \
	"GTKWave" \
	"gtkwave install hint"

rm -rf "$TMP7"

#########################################
# Test 8: Missing vvp
#########################################

TMP8=$(mktemp -d)

BIN="$TMP8/bin"
ROOTDIR="$TMP8/ARM_M3_design"

mkdir -p "$BIN"

# Create all tools except vvp
make_tool "$BIN" arm-none-eabi-gcc
make_tool "$BIN" arm-none-eabi-objcopy
make_tool "$BIN" arm-none-eabi-objdump
make_tool "$BIN" arm-none-eabi-size
make_tool "$BIN" qemu-system-arm
make_tool "$BIN" gdb-multiarch
make_tool "$BIN" iverilog
make_tool "$BIN" verilator
make_tool "$BIN" gtkwave

create_cortex_root "$ROOTDIR"

set +e
run_doctor_isolated "$BIN" "$ROOTDIR" "$TMP8/output.txt"
STATUS=$?
set -e

OUTPUT=$(cat "$TMP8/output.txt")

expect_exit 1 "$STATUS" \
	"missing vvp returns failure"

expect_contains "$OUTPUT" \
	"vvp" \
	"missing vvp detected"

rm -rf "$TMP8"

#########################################
# Test Summary
#########################################

printf "\n"
printf "Passed : %d\n" "$PASS"
printf "Failed : %d\n" "$FAIL"

if [ "$FAIL" -ne 0 ]; then
	exit 1
fi

printf "\nAll doctor tests passed.\n"
