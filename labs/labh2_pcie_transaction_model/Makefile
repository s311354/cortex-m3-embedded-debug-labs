TARGET := labh2_pcie_transaction_model
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
	  -I$(PCIE_PLATFORM_INC)

################################
# RTL simulation
################################
IVERILOG := iverilog
vvp      := vvp

RTL_BUILD := build

SHARED_RTL := \
	      $(PCIE_PLATFORM_RTL)/m3ds_pcie_host_wrapper.v \
	      $(PCIE_PLATFORM_RTL)/labh1_ahb_pcie_host_bridge.v

LABH2_RTL := \
	     rtl/m3ds_pcie_backend.v \
	     rtl/labh2_pcie_tlp_tx.v \
	     rtl/labh2_pcie_endpoint_model.v \
	     rtl/labh2_pcie_tlp_rx.v

RTL_SRC := \
	   $(SHARED_RTL) \
	   $(LABH2_RTL)


.PHONY: sim sim-backend wave wave-backend rtl-lint verify sim-clean
################################################
# Backend-only verification
################################################
sim-backend:
	mkdir -p $(RTL_BUILD)
	$(IVERILOG) \
		-g2012 \
		-Wall \
		-s tb_labh2_backend \
		-o $(RTL_BUILD)/labh2_backend_sim \
		$(LABH2_RTL) \
		tb/tb_labh2_backend.v
	$(VVP) $(RTL_BUILD)/labh2_backend_sim

wave-backend:
	gtkwave $(RTL_BUILD)/labh2_backend.vcd

################################################
# Standalone AHB end-to-end
################################################
sim:
	mkdir -p $(RTL_BUILD)
	$(IVERILOG) \
		-g2012 \
		-Wall \
		-s tb_labh2 \
		-o $(RTL_BUILD)/labh2_sim \
		$(RTL_SRC) \
		tb/tb_labh2.v
	$(vvp) $(RTL_BUILD)/labh2_sim

wave:
	gtkwave $(RTL_BUILD)/labh2.vcd

################################################
# Lint
################################################
rtl-lint:
	verilator \
		--lint-only \
		-Wall \
		--top-module m3ds_pcie_host_wrapper \
		$(RTL_SRC)


################################################
# Local RTL verification
################################################
verify:
	$(MAKE) rtl-lint
	$(MAKE) sim-backend
	$(MAKE) sim

sim-clean:
	rm -rf $(RTL_BUILD)

################################################
# ARM DesignStart Level 3
################################################
include sim/designstart.mk
