#!/bin/bash
#
# tests/test-resolve-lab.sh
#
# Unit tests for scripts/resolve-lab.sh
#
# Tests lab name resolution including:
# - Numeric lab identifiers (0, 1, 16, etc.)
# - Full lab names (lab15_spi_transaction, etc.)
# - New labs (17-19, h1)
# - Edge cases (leading zeros, invalid input)
# - Error handling and recovery hints
#

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
RESOLVER="${ROOT}/scripts/resolve-lab.sh"

PASS=0
FAIL=0

#########################################
# Test Assertion Helpers
#########################################

# Test exact string equality
expect_eq() {
	expected=$1
	actual=$2
	description=$3

	if [ "$expected" = "$actual" ]; then
		PASS=$((PASS + 1))
		printf '[PASS] %s\n' "$description"
	else
		FAIL=$((FAIL + 1))
		printf '[FAIL] %s\n' "$description"
		printf ' expected=%s\n' "$expected"
		printf ' actual=%s\n' "$actual"
	fi
}

# Test exit status
expect_exit() {
	expected=$1
	actual=$2
	description=$3

	if [ "$expected" -eq "$actual" ]; then
		PASS=$((PASS + 1))
		printf '[PASS] %s\n' "$description"
	else
		FAIL=$((FAIL + 1))
		printf '[FAIL] %s\n' "$description"
		printf ' expected exit=%s\n' "$expected"
		printf ' actual exit=%s\n' "$actual"
	fi
}

# Test output contains text
expect_contains() {
	output=$1
	text=$2
	description=$3

	case "$output" in
		*"$text"*)
			PASS=$((PASS + 1))
			printf '[PASS] %s\n' "$description"
			;;
		*)
			FAIL=$((FAIL + 1))
			printf '[FAIL] %s\n' "$description"
			printf ' expected output to contain: %s\n' "$text"
			;;
	esac
}

#########################################
# Test Group 1: Basic Numeric Resolution
#########################################

actual=$("$RESOLVER" 0)
expect_eq \
	"lab00_cross_compile" \
	"$actual" \
	"numeric lab 0 resolves"

actual=$("$RESOLVER" 1)
expect_eq \
	"lab01_core_registers" \
	"$actual" \
	"numeric lab 1 resolves"

actual=$("$RESOLVER" 16)
expect_eq \
	"lab16_hardware_spi_controller" \
	"$actual" \
	"numeric lab 16 resolves"

#########################################
# Test Group 2: New Labs (17-19, h1)
#########################################

actual=$("$RESOLVER" 17)
expect_eq \
	"lab17_pcie_host_config" \
	"$actual" \
	"numeric lab 17 resolves"

actual=$("$RESOLVER" 18)
expect_eq \
	"lab18_pcie_enumeration" \
	"$actual" \
	"numeric lab 18 resolves"

actual=$("$RESOLVER" 19)
expect_eq \
	"lab19_pcie_bar_resource_zephyr" \
	"$actual" \
	"numeric lab 19 resolves"

#########################################
# Test Group 3: Full Lab Names
#########################################

actual=$("$RESOLVER" lab15_spi_transaction)
expect_eq \
	"lab15_spi_transaction" \
	"$actual" \
	"full lab name resolves"

actual=$("$RESOLVER" lab00_cross_compile)
expect_eq \
	"lab00_cross_compile" \
	"$actual" \
	"lab00 full name resolves"

actual=$("$RESOLVER" lab17_pcie_host_config)
expect_eq \
	"lab17_pcie_host_config" \
	"$actual" \
	"lab17 full name resolves"

actual=$("$RESOLVER" labh1_ahb_pcie_host_bridge)
expect_eq \
	"labh1_ahb_pcie_host_bridge" \
	"$actual" \
	"labh1 special name resolves"

#########################################
# Test Group 4: Edge Cases
#########################################

# Leading zeros
actual=$("$RESOLVER" 01)
expect_eq \
	"lab01_core_registers" \
	"$actual" \
	"numeric lab 01 with leading zero resolves"

actual=$("$RESOLVER" 001)
expect_eq \
	"lab01_core_registers" \
	"$actual" \
	"numeric lab 001 with multiple leading zeros resolves"

#########################################
# Test Group 5: Error Cases
#########################################

set +e

output=$("$RESOLVER" 99 2>&1)
status=$?

set -e

expect_exit \
	1 \
	"$status" \
	"unknown numeric lab 99 fails"

expect_contains \
	"$output" \
	"make list" \
	"invalid lab gives recovery hint"

# Test invalid full name
set +e

output=$("$RESOLVER" lab99_nonexistent 2>&1)
status=$?

set -e

expect_exit \
	1 \
	"$status" \
	"unknown lab name fails"

expect_contains \
	"$output" \
	"make list" \
	"invalid lab name gives recovery hint"

#########################################
# Test Summary
#########################################

printf '\n'
printf 'Passed : %d\n' "$PASS"
printf 'Failed : %d\n' "$FAIL"

if [ "$FAIL" -ne 0 ]; then
	exit 1
fi

printf '\nAll resolve-lab tests passed.\n'
