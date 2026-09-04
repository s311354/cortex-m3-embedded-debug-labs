#
# LabH1 ARM DesignStart full-system integration
#
LABH1_ROOT := \
	      $(abspath $(dir $(lastwprd $(MAKEFILE_LIST)))/..)

ARM_M3_ROOT ?= \
	       $(abspath $(LABH1_ROOT)/../../../ARM_M3_design)

ARM_EXEC_TB := \
	       $(ARM_M3_ROOT)/m3designstart/logical/testbench/execution_tb

LABH1_BUILD := \
	       $(LABH1_ROOT)/build

LABH1_DS_FILELIST := \
		     $(LABH1_BUILD)/designstart_labh1.f

ARM_SIM ?= mti

####################################
# LabH1 RTL added to DesignStart
####################################

LABH1_DS_RTL := \
		$(LABH1_ROOT)/rtl/m3ds_pcie_host_wrapper.v \
		$(LABH1_ROOT)/rtl/labh1_ahb_pcie_host_bridge.v \
		$(LABH1_ROOT)/rtl/labh1_pcie_backend_stub.v

.PHONY: \
	designstart-filelist \
	designstart-baseline \
	designstart-compile \
	designstart-testcode \
	designstart-run \
	designstart

####################################
# Generate RTL file list
####################################

designstart-filelist:
	mkdir -p $(LABH1_BUILD)
	rm -f $(LABH1_DS_FILELIST)
	for src in $(LABH1_DS_RTL); do \
		printf "%s\n" "$$src" >> $(LABH1_DS_FILELIST); \
	done

####################################
# Baseline ARM DesignStart
####################################

designstart-baseline:
	$(MAKE) -C $(ARM_EXEC_TB) clean

	$(MAKE) -C $(ARM_EXEC_TB) \
		compile \
		SIMULATOR=$(ARM_SIM)

####################################
# DesignStart + LabH1 RTL
####################################

designstart-compile: designstart-filelist
	$(MAKE) -C $(ARM_EXEC_TB) clean

	$(MAKE) -C $(ARM_EXEC_TB) \
		compile \
		SIMULATOR=$(ARM_SIM) \
		BUILDOPTS="+define+M3DS_PCIE_HOST -f $(LABH1_DS_FILELIST)"

####################################
# Cortex-M3 smoke firmwave
####################################

designstart-testcode:
	$(MAKE) -C $(ARM_EXEC_TB) \
		testcode \
		TESTNAME=pcie_host_smoke \
		TOOL_CHAIN=gcc

####################################
# Run full-system simulation
####################################

designstart-run:
	$(MAKE) -C $(ARM_EXEC_TB) \
		run \
		TESTNAME=pcie_host_smoke \
		SIMULATOR=$(ARM_SIM)

####################################
# Full Level-3 verification
####################################

designstart:
	$(MAKE) designstart-compile
	$(MAKE) designstart-testcode
	$(MAKE) designstart-run

