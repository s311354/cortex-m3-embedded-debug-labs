#!/bin/sh
#
# tests/test-doctor.sh
#
# Unit tests for scripts/doctor.sh
#

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
DOCTOR="${ROOT}/scripts/doctor.sh"

PASS=0
FAIL=0

###########################
# Helpers
###########################

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

###########################
# Mock Toolchain
###########################

make_tool()
{
	dir="$1"
	tool="$2"

	cat > "${dir}/${tool}" << EOF

#!/bin/sh
exit 0
EOF
        chmod +x "${dir}/${tool}"
}

create_toolchain()
{
	dir="$1"

	make_tool "$dir" arm-none-eabi-gcc
	make_tool "$dir" arm-none-eabi-objcopy
	make_tool "$dir" arm-none-eabi-objdump
	make_tool "$dir" arm-none-eabi-size
	make_tool "$dir" qemu-system-arm
	make_tool "$dir" gdb-multiarch
}

###########################
# Mock ARM_M3_design
###########################

create_cortex_root()
{
	root=$1

	mkdir -p \
		"$root/m3designstart/software/cmsis/CMSIS/Include"

	mkdir -p \
		"$root/m3designstart/software/cmsis/Device/ARM/CM3DS/Include"

	touch \
		"$root/m3designstart/software/cmsis/CMSIS/Include/core_cm3.h"

	touch \
		"$root/m3designstart/software/cmsis/Device/ARM/CM3DS/Include/CM3DS_MPS2.h"
}


###########################
# Mock Execute doctor
###########################

run_doctor()
{
	PATH="$1" \
	CORTEX_ROOT="$2" \
	sh "$DOCTOR" > "$TMP/output.txt" 2>&1

	return $?
}

###########################
# Test 1
###########################

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
	"doctor succeeds"

expect_contains "$OUTPUT" \
	"Environment is ready" \
	"environment ready message"

###########################
# Test 2
###########################

TMP2=$(mktemp -d)

BIN="$TMP2/bin"
ROOTDIR="$TMP2/ARM_M3_design"

mkdir -p "$BIN"

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

###########################
# Test 3
###########################

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
	"missing ARM_M3_desing returns failure"

expect_contains "$OUTPUT" \
	"ARM_M3_design" \
	"missing cortex root detected"

expect_contains "$OUTPUT" \
	"CORTEX_ROOT" \
	"custom CORTEX_ROOT hint"

rm -rf "$TMP3"

###########################
# Test 4
###########################

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

###########################
# Summary
###########################

printf "\n"
printf "Passed : %d\n" "$PASS"
printf "Failed : %d\n" "$FAIL"

if [ "$FAIL" -ne 0 ]; then
	exit 1
fi

printf "\nAll doctor tests passed.\n"
