TARGET := lab18_pcie_enumeration
PLATFORM := selftest

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_MPS2_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Include
CMSIS_MPS2_SRC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Source
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

SELFTEST_ROOT := $(CORTEX_ROOT)/m3designstart/selftest
MPS2_INC := $(SELFTEST_ROOT)/v2m_mps2
CMSDK_DEVICE_INT := $(SELFTEST_ROOT)/cmsis/Device/Include
CMSDK_CM3_INC := $(SELFTEST_ROOT)/cmsis/Device/Include/CMSDK_CM3
CMSIS_CORE_INC := $(SELFTEST_ROOT)/cmsis/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
$(CMSIS_MPS2_SRC)/system_CM3DS.c \
main.c \
runtime.c \
pcie_host.c \
pcie_bus.c \
pcie_sim_host.c \
board_pcie.c \
startup.s

VPATH += $(COMMON)

include $(COMMON)/Makefile.common









