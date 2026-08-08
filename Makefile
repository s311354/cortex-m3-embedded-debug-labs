# ===============================
# Cortex-M3 Embedded Debug Labs
# ===============================
.DEFAULT_GOAL := help

PROJECT_ROOT := $(CURDIR)
export PROJECT_ROOT

LABS := \
       lab00_cross_compile \
       lab01_core_registers \
       lab02_interrupt_control \
       lab03_svc_exception \
       lab04_stack_frame \
       lab05_privilege_stack \
       lab06_startup_runtime \
       lab07_optimization \
       lab08_uart_register \
       lab09_uart_polling \
       lab10_uart_interrupt \
       lab11_uart_ringbuffer \
       lab12_uart_driver_abstraction \
       lab13_i2c_transaction \
       lab14_mps2_mmio_i2c \
       lab15_spi_transaction \
       lab16_hardware_spi_controller

LAB_DIR := labs

.PHONY: all clean run debug gdb dump doctor test help list $(LABS)

doctor:
	@CORTEX_ROOT="${CORTEX_ROOT}" ./scripts/doctor.sh

test:
	@sh tests/test-doctor.sh
	@sh tests/test-resolve-lab.sh

# ===============================
# Build every lab
# ===============================
all:
	@for lab in $(LABS); do \
		echo "=========================================="; \
		echo "Building $$lab"; \
		echo "=========================================="; \
                $(MAKE) -C $(LAB_DIR)/$$lab || exit $$?; \
	done

# ===============================
# Clean every lab
# ===============================
clean:
	@for lab in $(LABS); do \
		echo "Cleaning $$lab"; \
		$(MAKE) -C $(LAB_DIR)/$$lab clean; \
	done

# ===============================
# Build a single lab
# Example:
#     make lab00_cross_compile
# ===============================
$(LABS):
	@$(MAKE) -C $(LAB_DIR)/$@

# ===============================
# Show supported labs
# ===============================
list:
	@printf "%s\n" $(LABS)

# ===============================
# Run / Debug / GDB
# ===============================
RESOLVE_LAB := ./scripts/resolve-lab.sh

ifeq ($(LAB),)
run:
	$(error Usage: make run LAB=<lab_name|number>)
debug:
	$(error Usage: make debug LAB=<lab_name|number>)
gdb:
	$(error Usage: make gdb LAB=<lab_name|number>)
dump:
	$(error Usage: make dump LAB=<lab_name|number>)
else
run:
	@lab="$$( $(RESOLVE_LAB) "$(LAB)" )" && \
	$(MAKE) -C $(LAB_DIR)/$$lab run
debug:
	@lab="$$( $(RESOLVE_LAB) "$(LAB)" )" && \
	$(MAKE) -C $(LAB_DIR)/$$lab debug
gdb:
	@lab="$$( $(RESOLVE_LAB) "$(LAB)" )" && \
	$(MAKE) -C $(LAB_DIR)/$$lab gdb
dump:
	@lab="$$( $(RESOLVE_LAB) "$(LAB)" )" && \
	$(MAKE) -C $(LAB_DIR)/$$lab dump
endif

# ===============================
# Help
# ===============================
help:
	@echo ""
	@echo "Cortex-M3 Embedded Debug Labs"
	@echo ""
	@echo "Usage:"
	@echo " make doctor     Check toolchain and external dependencies"
	@echo " make test       Run repository unit tests"
	@echo " make            Build all labs"
	@echo " make all        Build all labs"
	@echo " make clean      Clean all labs"
	@echo " make list       List all labs "
	@echo " make run LAB=<lab_name>       "
	@echo " make debug LAB=<lab_name>     "
	@echo " make gdb LAB=<lab_name>       "
	@echo " make dump LAB=<lab_name>       "
	@echo ""
	@echo "Build a single lab:"
	@for lab in $(LABS); do \
		echo " make $$lab"; \
	done
	@echo ""
