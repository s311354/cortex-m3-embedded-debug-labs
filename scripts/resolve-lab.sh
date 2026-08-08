#!/bin/bash

set -eu

ROOT=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
LABS_DIR="${ROOT}/labs"

usage() {
	printf 'Usage: %s <lab_name|number>\n' "$0" >&2
}

if [ "$#" -ne 1 ]; then
	usage
	exit 2
fi

input=$1

case "$input" in
	*[!0-9]*)
		lab_name=$input
		;;

	*)
		lab_num=$(printf '%02d' "$((10#$input))")

		set -- "${LABS_DIR}/lab${lab_num}_"*

		if [ "$#" -ne 1 ] || [ ! -d "$1" ]; then
		       printf \
		           'Error: lab %s not found. Run "make list" to see available labs.\n' \
	                   "$input" >&2
                       exit 1
                fi

                lab_name=$(basename "$1")
                ;;
esac

if [ ! -d "${LABS_DIR}/${lab_name}" ]; then
	printf \
		'Error: lab "%s" not found. Run "make list" to see available labs.\n' \
		"$input" >&2
	exit 1
fi

printf '%s\n' "$lab_name"

