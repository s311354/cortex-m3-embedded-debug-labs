#
# LabH1 ARM DesignStart full-system integration
#
LABH1_ROOT := \
	      $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)

ARM_M3_ROOT ?= \
	       $(abspath $(LABH1_ROOT)/../../../ARM_M3_design)

ARM_TESTBENCH := \
		 $(ARM_M3_ROOT)/m3designstart/logical/testbench

ARM_EXEC_TB := \
	       $(ARM_TESTBENCH)/execution_tb

LABH1_BUILD := \
	       $(LABH1_ROOT)/build

LABH1_DS_FILELIST := \
		     $(LABH1_BUILD)/designstart_labh1.f

ARM_SIM ?= mti
SIM_64BIT ?= no
TESTNAME ?= pcie_host_smoke
TOOL_CHAIN ?= gcc

# Testcode paths
TESTCODE_DIR := $(ARM_TESTBENCH)/testcodes/$(TESTNAME)
TESTCODE_BIN := $(TESTCODE_DIR)/$(TESTNAME).bin
TESTCODE_ELF := $(TESTCODE_DIR)/$(TESTNAME).elf

####################################
# LabH1 RTL added to DesignStart
####################################

# Shared platform RTL (common across labs)
PLATFORM_PCIE_RTL := \
		     $(LABH1_ROOT)/../../platform/pcie/rtl

LABH1_DS_RTL := \
		$(PLATFORM_PCIE_RTL)/m3ds_pcie_host_wrapper.v \
		$(PLATFORM_PCIE_RTL)/labh1_ahb_pcie_host_bridge.v \
		$(LABH1_ROOT)/rtl/m3ds_pcie_backend.v \
		$(LABH1_ROOT)/rtl/labh1_pcie_backend_stub.v

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
		SIM_64BIT=$(SIM_64BIT) \
		BUILDOPTS="+define+M3DS_PCIE_HOST -f $(LABH1_DS_FILELIST)"

####################################
# Cortex-M3 CPU-side smoke firmwave
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
	@mkdir -p $(LABH1_BUILD)
	$(MAKE) -C $(ARM_EXEC_TB) \
		testcode \
		TESTNAME=$(TESTNAME) \
		TOOL_CHAIN=$(TOOL_CHAIN) \
		2>&1 | tee $(LABH1_BUILD)/firmware_build.log
	@if [ ! -f "$(TESTCODE_BIN)" ]; then \
		echo "ERROR: Firmware build failed - $(TESTNAME).bin not found"; \
		echo "Check log: $(LABH1_BUILD)/firmware_build.log"; \
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
	@rm -rf $(LABH1_BUILD)
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
	@mkdir -p $(LABH1_BUILD)/logs
	$(MAKE) -C $(ARM_EXEC_TB) \
		run \
		TESTNAME=$(TESTNAME) \
		SIMULATOR=$(ARM_SIM) \
		SIM_64BIT=$(SIM_64BIT) \
		2>&1 | tee $(LABH1_BUILD)/logs/simulation_$$(date +%Y%m%d_%H%M%S).log

####################################
# Full Level-3 verification
####################################

designstart:
	$(MAKE) designstart-compile
	$(MAKE) designstart-firmware
	$(MAKE) designstart-run

# Complete clean build and simulation
designstart-all: designstart-clean
	$(MAKE) designstart-compile
	$(MAKE) designstart-firmware
	$(MAKE) designstart-run
