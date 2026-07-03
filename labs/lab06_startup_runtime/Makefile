TARGET := lab06_startup_runtime
PLATFORM := runtime

COMMON := $(PROJECT_ROOT)/platform/baremetal
RUNTIME := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

LINKER_SCRIPT := $(RUNTIME)/linker.ld

SRC := \
main.c \
runtime.c \
startup.s

VPATH += $(RUNTIME)

include $(COMMON)/Makefile.common
