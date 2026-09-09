TARGET := labh1_ahb_pcie_host_bridge
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
pcie_ahb_host.c \
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
	      $(PCIE_PLATFORM_RTL)/labh1_ahb_pcie_host_bridge.v \

LABH1_RTL := \
	     rtl/m3ds_pcie_backend.v \
	     rtl/labh1_pcie_backend_stub.v

RTL_SRC := \
	   $(SHARED_RTL) \
	   $(LABH1_RTL)

.PHONY: sim wave rtl-lint sim-clean

################################################
# Standalone AHB end-to-end
################################################
TB_SRC := \
tb/tb_labh1.v

sim:
	mkdir -p $(RTL_BUILD)
	$(IVERILOG) \
		-g2012 \
		-Wall \
		-o $(RTL_BUILD)/labh1_sim \
		$(RTL_SRC) \
		$(TB_SRC)
	$(vvp) $(RTL_BUILD)/labh1_sim

wave:
	gtkwave $(RTL_BUILD)/labh1.vcd

################################################
# Lint
################################################
rtl-lint:
	verilator \
		--lint-only \
		-Wall \
		$(RTL_SRC)

################################################
# Local RTL verification
################################################
verify:
	$(MAKE) rtl-lint
	$(MAKE) sim

sim-clean:
	rm -rf $(RTL_BUILD) modelsim_env.sh modelsim_wrapper.sh Makefile.modelsim modelsim_bin

################################################
# ARM DesignStart Level 3
################################################
include sim/designstart.mk
