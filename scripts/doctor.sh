#!/bin/sh

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CORTEX_ROOT="${CORTEX_ROOT:-${ROOT}/../ARM_M3_design}"

errors=0

check_command() {
	local command_name="$1"
	local install_hint="$2"
	
	if command -v "${command_name}" >/dev/null 2>&1; then
		printf '[PASS] %s\n' "${command_name}"
	else
		printf '[FAIL] %s not found. %s\n' "${command_name}" "${install_hint}"
	        errors=$((errors + 1))
	fi	     	
}

check_directory() {
	local label="$1"
	local directory="$2"
	local hint="$3"

	if [ -d "${directory}" ]; then
		printf '[PASS] %s: %s\n' "${label}" "${directory}"
	else
		printf '[FAIL] %s not found: %s\n' "${label}" "${directory}"
		printf '       %s\n' "${hint}"
		errors=$((errors + 1))
	fi
}

check_command arm-none-eabi-gcc \
	"Install the Arm GNU Embedded Toolchain."
check_command arm-none-eabi-objcopy \
	"Install the Arm GUN Embedded Toolchain."
check_command arm-none-eabi-objdump \
	"Install the Arm GUN Embedded Toolchain."
check_command arm-none-eabi-size \
	"Install the Arm GUN Embedded Toolchain."
check_command qemu-system-arm \
	"Install qemu-system-arm."
check_command gdb-multiarch \
	"Install gdb-multiarch."

check_directory \
	"ARM_M3_design" \
	"${CORTEX_ROOT}" \
	"Clone ARM_M3_design beside this repository or set CORTEX_ROOT."

check_directory \
	"CMSIS core headers" \
	"${CORTEX_ROOT}/m3designstart/software/cmsis/CMSIS/Include" \
	"Verify that CORTEX_ROOT points to a complete ARM_M3_design checkout."

printf '\n'

if [ "${errors}" -gt 0 ]; then
	printf 'Environment check failed with %d problem(s).\n' "${errors}"
	exit 1
fi

printf 'Environment is ready.\n'
