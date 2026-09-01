TARGET := labh1_ahb_pcie_host_bridge
PLATFORM := runtime

# Set PROJECT_ROOT if not already set (when running make from lab directory)
PROJECT_ROOT ?= $(realpath ../..)

COMMON := $(PROJECT_ROOT)/platform/$(PLATFORM)

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


################################
# RTL simulation
################################
IVERILOG := iverilog
vvp      := vvp

RTL_BUILD := build

RTL_SRC := \
rtl/m3ds_pcie_host_wrapper.v \
rtl/labh1_ahb_pcie_host_bridge.v \
rtl/labh1_pcie_backend_stub.v

TB_SRC := \
tb/tb_labh1.v

.PHONY: sim wave rtl-lint sim-clean

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

rtl-lint:
	verilator \
		--lint-only \
		-Wall \
		$(RTL_SRC)

sim-clean:
	rm -rf $(RTL_BUILD)

