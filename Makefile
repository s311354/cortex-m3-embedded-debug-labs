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
       lab12_uart_driver_abstraction

LAB_DIR := labs

.PHONY: all clean run debug gdb dump help list $(LABS)

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

ifeq ($(LAB),)
run:
	$(error Usage: make run LAB=<lab_name>)
debug:
	$(error Usage: make debug LAB=<lab_name>)
gdb:
	$(error Usage: make gdb LAB=<lab_name>)
dump:
	$(error Usage: make dump LAB=<lab_name>)
else
run:
	$(MAKE) -C $(LAB_DIR)/$(LAB) run
debug:
	$(MAKE) -C $(LAB_DIR)/$(LAB) debug
gdb:
	$(MAKE) -C $(LAB_DIR)/$(LAB) gdb
dump:
	$(MAKE) -C $(LAB_DIR)/$(LAB) dump
endif

# ===============================
# Help
# ===============================
help:
	@echo ""
	@echo "Cortex-M3 Embedded Debug Labs"
	@echo ""
	@echo "Usage:"
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
