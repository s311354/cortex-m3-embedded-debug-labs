TARGET := lab25_watchdog_heartbeat_zephyr
PLATFORM := zephyr

PROJECT_ROOT ?= $(abspath ../..)
COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

BOARD := mps2/an385
BUILD_DIR := build

# Include common build rules
include $(COMMON)/Makefile.common

.PHONY: test

test:
	@bash tests/run_host.sh
