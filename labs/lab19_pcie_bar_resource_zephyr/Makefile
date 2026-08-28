TARGET := lab19_pcie_bar_resource_zephyr
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


ZEPHYR_TOP := $(shell west topdir)
ZEPHYR_ROOT := $(ZEPHYR_TOP)/zephyr
ZEPHYR_BUILD := build

ZEPHYR_INC := $(ZEPHYR_ROOT)/include
ZEPHYR_CMSIS6_INC := $(ZEPHYR_ROOT)/modules/cmsis_6
ZEPHYR_CMSIS6_CORE_INC := $(ZEPHYR_TOP)/modules/hal/cmsis_6/CMSIS/Core/Include
ZEPHYR_SOC_INC := $(ZEPHYR_ROOT)/soc/arm/mps2
ZEPHYR_GEN_INC := $(ZEPHYR_BUILD)/zephyr/include/generated
ZEPHYR_GEN_ZEPHYR_INC := $(ZEPHYR_GEN_INC)/zephyr
ZEPHYR_AUTOCONF := $(ZEPHYR_GEN_ZEPHYR_INC)/autoconf.h


LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
$(CMSIS_MPS2_SRC)/system_CM3DS.c \
$(COMMON)/runtime.c \
main.c \
pcie_host.c \
pcie_resource.c \
pcie_sim_host.c \
board_pcie.c \
$(COMMON)/startup.s

VPATH += $(COMMON)

# Set default goal to our custom all target
.DEFAULT_GOAL := all

# West build dependency - ensure Zephyr headers are generated before compilation
.PHONY: west-build all
west-build:
	@echo "Running west build to generate Zephyr dependencies..."
	@PROJECT_ROOT=$(PROJECT_ROOT) $(PROJECT_ROOT)/scripts/west-build.sh -b mps2/an385 -s $(ZEPHYR_ROOT)/samples/hello_world
	@if [ ! -f "$(ZEPHYR_AUTOCONF)" ]; then \
		echo "Error: Zephyr autoconf.h not generated at $(ZEPHYR_AUTOCONF)"; \
		exit 1; \
	fi
	@echo "Zephyr dependencies ready: $(ZEPHYR_AUTOCONF)"

# Custom all target that runs west-build first
all: west-build
	@$(MAKE) $(TARGET).elf $(TARGET).bin

# Make all source compilations depend on west-build (order-only prerequisite)  
%.o: | west-build

# Include common build rules
include $(COMMON)/Makefile.common









