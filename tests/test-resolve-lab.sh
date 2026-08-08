#!/bin/bash

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
RESOLVER="${ROOT}/scripts/resolve-lab.sh"

PASS=0
FAIL=0

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

actual=$("$RESOLVER" 16)

expect_eq \
	"lab16_hardware_spi_controller" \
	"$actual" \
	"numeric lab 16 resolves"

actual=$("$RESOLVER" 0)

expect_eq \
	"lab00_cross_compile" \
	"$actual" \
	"numeric lab 0 resolves"

actual=$("$RESOLVER" lab15_spi_transaction)

expect_eq \
	"lab15_spi_transaction" \
	"$actual" \
	"full lab name resolves"

set +e

output=$("$RESOLVER" 99 2>&1)
status=$?

set -e

expect_eq \
	"1" \
	"$status" \
	"unknown numeric lab fails"


case "$output" in
	*'make list'*)
		PASS=$((PASS + 1))
		printf '[PASS] invalid lab gives recovery hint\n'
		;;
	*)
		FAIL=$((FAIL + 1))
		printf '[FAIL] invalid lab gives recovery hint\n'
		;;
esac

printf '\nPassed : %d\n' "$PASS"
printf 'Failed : %d\n' "$FAIL"

[ "$FAIL" -eq 0 ]
