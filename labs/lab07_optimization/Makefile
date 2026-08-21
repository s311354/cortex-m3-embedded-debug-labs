TARGET := lab07_optimization
PLATFORM := runtime

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_MPS2_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Include
CMSIS_MPS2_SRC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Source
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
startup.s \
$(CMSIS_MPS2_SRC)/system_CM3DS.c \
main.c \
runtime.c 

VPATH += $(COMMON)

include $(COMMON)/Makefile.common
