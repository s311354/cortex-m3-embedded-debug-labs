#!/usr/bin/env bash
set -euo pipefail
LAB_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
BUILD=$(mktemp -d "${TMPDIR:-/tmp}/lab25-host.XXXXX")
trap 'rm -rf "$BUILD"' EXIT
CC=${CC:-cc}
read -r -a EXTRA_FLAGS <<< "${CFLAGS:--O2}"
"$CC" -std=c11 -Wall -Wextra -Werror -Wpedantic -pthread \
    "${EXTRA_FLAGS[@]}" \
    -I"$LAB_DIR/tests/include" -I"$LAB_DIR/src" \
    "$LAB_DIR/src/watchdog_supervisor.c" "$LAB_DIR/tests/host_contract.c" \
    -o "$BUILD/host_contract"
"$BUILD/host_contract"
