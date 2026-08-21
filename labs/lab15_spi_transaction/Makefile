TARGET := lab14_spi_transaction
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
runtime.c \
main.c \
simulated_spi_device.c \
spi_bitbang.c

VPATH += $(COMMON)

include $(COMMON)/Makefile.common









