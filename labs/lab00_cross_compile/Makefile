TARGET := lab00_cross_compile
PLATFORM := baremetal

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
main.c \
startup.s

VPATH += $(COMMON)

include $(COMMON)/Makefile.common









