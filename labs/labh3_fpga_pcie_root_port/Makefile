TARGET := labh3_fpge_pcie_root_port
PLATFORM := runtime

# Set PROJECT_ROOT if not already set (when running make from lab directory)
PROJECT_ROOT ?= $(realpath ../..)

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

PCIE_PLATFORM := \
		 $(PROJECT_ROOT)/platform/pcie

PCIE_PLATFORM_RTL := \
		     $(PCIE_PLATFORM)/rtl

PCIE_PLATFORM_INC := \
		     $(PCIE_PLATFORM)/include

CORTEX_ROOT := ../../../ARM_M3_design
CMSIS_MPS2_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Include
CMSIS_MPS2_SRC := $(CORTEX_ROOT)/m3designstart/software/cmsis/Device/ARM/CM3DS/Source
CMSIS_INC := $(CORTEX_ROOT)/m3designstart/software/cmsis/CMSIS/Include

LINKER_SCRIPT := $(COMMON)/linker.ld

SRC := \
startup.s \
$(CMSIS_MPS2_SRC)/system_CM3DS.c \
syscalls.c \
main.c \
runtime.c 

VPATH += $(COMMON)

include $(COMMON)/Makefile.common

CFLAGS += \
	  -I$(PCIE_PLATFORM_INC) \
	  -Iinclude

################################
# RTL simulation
################################
IVERILOG := iverilog
vvp      := vvp

RTL_BUILD := build

SHARED_RTL := \
	      $(PCIE_PLATFORM_RTL)/labh1_ahb_pcie_host_bridge.v

LABH3_RTL := \
	     rtl/labh3_ahb_pcie_mmio_bridge.v \
	     rtl/labh3_pcie_root_port_wrapper.v \
	     rtl/m3ds_pcie_backend.v \

RTL_CONTRACT_SRC := \
             $(SHARED_RTL) \
             $(LABH3_RTL) \
	     rtl/labh3_pcie_vendor_adapter.v

RTL_SRC := \
	   $(SHARED_RTL) \
	   $(LABH3_RTL) \
	   rtl/labh3_pcie_vendor_adapter_model.v


.PHONY: sim sim-backend wave wave-backend rtl-lint-contract rtl-lint-sim rtl-lint verify sim-clean



################################################
# Standalone AHB end-to-end
################################################
sim:
	mkdir -p $(RTL_BUILD)
	$(IVERILOG) \
		-g2012 \
		-Wall \
		-s tb_labh3 \
		-o $(RTL_BUILD)/labh3_sim \
		$(RTL_SRC) \
		tb/tb_labh3.v
	$(vvp) $(RTL_BUILD)/labh3_sim

wave:
	gtkwave $(RTL_BUILD)/labh3.vcd

################################################
# Lint
################################################
rtl-lint-contract:
	verilator \
		--lint-only \
		-Wall \
		--top-module labh3_pcie_root_port_wrapper \
		$(RTL_CONTRACT_SRC)

rtl-lint-sim:
	verilator \
		--lint-only \
		-Wall \
		--top-module labh3_pcie_root_port_wrapper \
		$(RTL_SRC)

rtl-lint:
	$(MAKE) rtl-lint-contract
	$(MAKE) rtl-lint-sim

################################################
# Local RTL verification
################################################
verify:
	$(MAKE) rtl-lint
	$(MAKE) sim

sim-clean:
	rm -rf $(RTL_BUILD)

################################################
# ARM DesignStart Level 3
################################################
include sim/designstart.mk
