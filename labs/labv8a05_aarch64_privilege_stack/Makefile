# ==============================
# Lab v8a04
# AArch64 Privilege Stack
# ==============================

TARGET := labv805_aarch64_privilege_stack

PROJECT_ROOT ?= $(abspath ../..)

PLATFORM := aarch64

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

include $(COMMON)/Makefile.common

CFLAGS += -mgeneral-regs-only
