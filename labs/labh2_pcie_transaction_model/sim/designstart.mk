#
# LabH2 ARM DesignStart full-system verification
#
LABH2_ROOT := \
	      $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

ARM_M3_ROOT ?= \
	       $(abspath $(LABH2_ROOT)/../../../ARM_M3_design)

ARM_TESTBENCH := \
		 $(ARM_M3_ROOT)/m3designstart/logical/testbench

ARM_EXEC_TB := \
	       $(ARM_TESTBENCH)/execution_tb

LABH2_BUILD := \
	       $(LABH2_ROOT)/build

LABH2_DS_FILELIST := \
		     $(LABH2_BUILD)/designstart_labh2.f

ARM_SIM ?= mti
SIM_64BIT ?= no
TESTNAME ?= pcie_host_labh2
TOOL_CHAIN ?= gcc

# Container ModelSim: Additional directories to mount
# Default: Root of cortex-m3-embedded-debug-labs project AND ARM_M3_design
MODELSIM_EXTRA_MOUNTS ?= $(abspath $(LABH2_ROOT)/../..) $(ARM_M3_ROOT)

# Testcode paths
TESTCODE_DIR := $(ARM_TESTBENCH)/testcodes/$(TESTNAME)
TESTCODE_BIN := $(TESTCODE_DIR)/$(TESTNAME).bin
TESTCODE_ELF := $(TESTCODE_DIR)/$(TESTNAME).elf

####################################
# LabH2 RTL added to DesignStart
####################################

# Shared platform RTL (common across labs)
PLATFORM_PCIE_RTL := \
		     $(LABH2_ROOT)/../../platform/pcie/rtl

LABH2_DS_RTL := \
		$(PLATFORM_PCIE_RTL)/m3ds_pcie_host_wrapper.v \
		$(PLATFORM_PCIE_RTL)/labh1_ahb_pcie_host_bridge.v \
		$(LABH2_ROOT)/rtl/m3ds_pcie_backend.v \
		$(LABH2_ROOT)/rtl/labh2_pcie_tlp_tx.v \
		$(LABH2_ROOT)/rtl/labh2_pcie_endpoint_model.v \
		$(LABH2_ROOT)/rtl/labh2_pcie_tlp_rx.v

.PHONY: \
	designstart-filelist \
	designstart-baseline \
	designstart-compile \
	designstart-firmware \
	designstart-firmware-check \
	designstart-clean \
	designstart-run \
	designstart \
	designstart-all

####################################
# Generate RTL file list
####################################

designstart-filelist:
	mkdir -p $(LABH2_BUILD)
	rm -f $(LABH2_DS_FILELIST)
	for src in $(LABH2_DS_RTL); do \
		printf "%s\n" "$$src" >> $(LABH2_DS_FILELIST); \
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
# DesignStart + LabH2 RTL backend
####################################

designstart-compile: designstart-filelist
	$(MAKE) -C $(ARM_EXEC_TB) clean

	$(MAKE) -C $(ARM_EXEC_TB) \
		compile \
		SIMULATOR=$(ARM_SIM) \
		SIM_64BIT=$(SIM_64BIT) \
		BUILDOPTS="+define+M3DS_PCIE_HOST -f $(LABH2_DS_FILELIST)" \

####################################
# Cortex-M3 CPU-side firmwave
####################################

# Check if testcode source exists
designstart-firmware-check:
	@if [ ! -f "$(TESTCODE_DIR)/$(TESTNAME).c" ]; then \
		echo "ERROR: Test source not found: $(TESTCODE_DIR)/$(TESTNAME).c"; \
		exit 1; \
	fi
	@echo "Testcode source: $(TESTCODE_DIR)/$(TESTNAME).c"

# Build firmware using ARM's testcode build system
designstart-firmware: designstart-firmware-check
	@echo "========================================"
	@echo "Building firmware: $(TESTNAME)"
	@echo "========================================"
	@mkdir -p $(LABH2_BUILD)
	$(MAKE) -C $(ARM_EXEC_TB) \
		testcode \
		TESTNAME=$(TESTNAME) \
		TOOL_CHAIN=$(TOOL_CHAIN) \
		2>&1 | tee $(LABH2_BUILD)/firmware_build.log
	@if [ ! -f "$(TESTCODE_BIN)" ]; then \
		echo "ERROR: Firmware build failed - $(TESTNAME).bin not found"; \
		echo "Check log: $(LABH2_BUILD)/firmware_build.log"; \
		exit 1; \
	fi
	@echo "========================================"
	@echo "Firmware build complete!"
	@echo "  BIN: $(TESTCODE_BIN) ($$(stat -c%s $(TESTCODE_BIN) 2>/dev/null || echo '?') bytes)"
	@if [ -f "$(TESTCODE_ELF)" ]; then \
		echo "  ELF: $(TESTCODE_ELF)"; \
	fi
	@echo "========================================"

# Clean build artifacts
designstart-clean:
	@echo "Cleaning all build artifacts..."
	@echo "  - Firmware artifacts for $(TESTNAME)"
	@if [ -d "$(TESTCODE_DIR)" ]; then \
		cd $(TESTCODE_DIR) && make clean 2>/dev/null || true; \
		rm -f $(TESTCODE_DIR)/*.o $(TESTCODE_DIR)/*.elf $(TESTCODE_DIR)/*.bin \
		      $(TESTCODE_DIR)/*.hex $(TESTCODE_DIR)/*.lst $(TESTCODE_DIR)/*.map; \
	fi
	@echo "  - Lab build directory"
	@rm -rf $(LABH2_BUILD)
	@echo "  - RTL compilation artifacts"
	@if [ -d "$(ARM_EXEC_TB)" ]; then \
		$(MAKE) -C $(ARM_EXEC_TB) clean 2>/dev/null || true; \
	fi
	@echo "All build artifacts cleaned."

####################################
# Run full-system simulation
####################################

designstart-run:
	@echo "========================================"
	@echo "Running simulation: $(TESTNAME)"
	@echo "========================================"
	@mkdir -p $(LABH2_BUILD)/logs
	export MODELSIM_EXTRA_MOUNTS="$(MODELSIM_EXTRA_MOUNTS)"; \
	$(MAKE) -C $(ARM_EXEC_TB) \
		run \
		TESTNAME=$(TESTNAME) \
		SIMULATOR=$(ARM_SIM) \
		SIM_64BIT=$(SIM_64BIT) \
		2>&1 | tee $(LABH2_BUILD)/logs/simulation_$$(date +%Y%m%d_%H%M%S).log

####################################
# Generic H1/H2 compatibility smoke
####################################
designstart-smoke:
	$(MAKE) designstart-compile
	$(MAKE) designstart-firmware \
		TESTNAME=pcie_host_smoke
	$(MAKE) designstart-run \
		TESTNAME=pcie_host_smoke

####################################
# Full Level-3 verification
####################################

designstart:
	$(MAKE) designstart-compile
	$(MAKE) designstart-firmware \
		TESTNAME=pcie_host_labh2
	$(MAKE) designstart-run \
		TESTNAME=pcie_host_labh2

# Complete clean build and simulation
designstart-all: designstart-clean
	$(MAKE) designstart-baseline
	$(MAKE) designstart-smoke
	$(MAKE) designstart
