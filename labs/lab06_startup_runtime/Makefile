TARGET := lab06_startup_runtime
PLATFORM := runtime

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_MPS2 := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Include
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
startup.s \
syscalls.c \
main.c \
runtime.c 

VPATH += $(COMMON)

include $(COMMON)/Makefile.common
