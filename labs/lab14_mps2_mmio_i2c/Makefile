TARGET := lab14_mps2_mmio_i2c
PLATFORM := selftest

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_MPS2 := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Include
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

SELFTEST_ROOT := $(CORTEX_ROOT)/m3designstart/selftest
MPS2_INC := $(SELFTEST_ROOT)/v2m_mps2
CMSDK_DEVICE_INT := $(SELFTEST_ROOT)/cmsis/Device/Include
CMSDK_CM3_INC := $(SELFTEST_ROOT)/cmsis/Device/Include/CMSDK_CM3
CMSIS_CORE_INC := $(SELFTEST_ROOT)/cmsis/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
main.c \
runtime.c \
mps2_i2c.c \
eeprom.c \
board_device.c \
startup.s

VPATH += $(COMMON)

include $(COMMON)/Makefile.common









