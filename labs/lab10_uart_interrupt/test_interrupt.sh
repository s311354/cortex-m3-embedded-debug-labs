#!/bin/bash
# Test script for lab10_uart_interrupt

echo "Testing UART interrupt handler..."
echo "Type some characters. They should be echoed back."
echo "Press Ctrl-A then X to exit QEMU"
echo ""

export PROJECT_ROOT=/home/shelton/cortex-m3-embedded-debug-labs
cd /home/shelton/cortex-m3-embedded-debug-labs/labs/lab10_uart_interrupt

qemu-system-arm \
    -M mps2-an385 \
    -cpu cortex-m3 \
    -nographic \
    -serial mon:stdio \
    -kernel lab10_uart_interrupt.elf
