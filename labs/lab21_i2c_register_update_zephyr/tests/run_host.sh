#!/usr/bin/env bash
set -euo pipefail

LAB_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/lab23-flash-host.XXXXXX")
trap 'rm -rf -- "$BUILD"' EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

CC=${HOST_CC:-${CC:-cc}}

read -r -a EXTRA_FLAGS <<< "${HOST_CFLAGS:-${CFLAGS:--O2}}"

"$CC" -std=c11 -Wall -Wextra -Werror -Wpedantic \
    "${EXTRA_FLAGS[@]}" \
    -I"$LAB_DIR/tests/include" -I"$LAB_DIR/src" \
    "$LAB_DIR/src/reg_access.c" "$LAB_DIR/tests/host_contract.c" \
    -o "$BUILD/host_contract"

"$BUILD/host_contract"
