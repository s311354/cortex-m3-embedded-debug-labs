#!/bin/bash
# Quick verification script for Lab H2
# Usage: ./verify.sh [--wave]

set -e

echo "==================================================="
echo "Lab H2 Hardware Verification Script"
echo "==================================================="
echo ""

# Step 1: Clean previous build
echo "[1/4] Cleaning previous build..."
make sim-clean 2>/dev/null || true
echo "      ✓ Clean complete"
echo ""

# Step 2: Run simulation
echo "[2/4] Running RTL simulation..."
if make sim 2>&1 | tee /tmp/labh2_sim.log; then
    echo "      ✓ Simulation complete"
else
    echo "      ✗ Simulation FAILED"
    exit 1
fi
echo ""

# Step 3: Check results
echo "[3/4] Verifying results..."

# Check for any failures first
if grep -q "LABH2 FAIL" /tmp/labh2_sim.log; then
    echo "      ✗ FAIL: Tests failed"
    grep "LABH2 FAIL" /tmp/labh2_sim.log
    exit 1
fi

# Check for all expected PASS messages
PASS_COUNT=$(grep -c "LABH2 PASS" /tmp/labh2_sim.log || echo "0")

if [ "$PASS_COUNT" -lt 3 ]; then
    echo "      ✗ FAIL: Expected 3 passing tests, found $PASS_COUNT"
    exit 1
fi

echo "      ✓ All tests passed ($PASS_COUNT/3)"
echo ""

# Test 1: CFG_READ - Device ID
if grep -q "LABH2 PASS: CFG_READ 00:01.0 -> 56781234" /tmp/labh2_sim.log; then
    echo "      ✓ Test 1: CFG_READ device ID = 0x56781234"
    echo "        - Vendor ID: 0x1234"
    echo "        - Device ID: 0x5678"
else
    echo "      ✗ Test 1: CFG_READ failed or wrong value"
    exit 1
fi

# Test 2: CFG_WRITE/READBACK
if grep -q "LABH2 PASS: CFG_WRITE/READBACK -> a5a55a5a" /tmp/labh2_sim.log; then
    echo "      ✓ Test 2: CFG_WRITE/READBACK = 0xA5A55A5A"
else
    echo "      ✗ Test 2: CFG_WRITE/READBACK failed"
    exit 1
fi

# Test 3: Absent device detection
if grep -q "LABH2 PASS: absetn BDF -> FFFFFFFF" /tmp/labh2_sim.log; then
    echo "      ✓ Test 3: Absent BDF detection = 0xFFFFFFFF"
else
    echo "      ✗ Test 3: Absent BDF detection failed"
    exit 1
fi

# Final verification message
if grep -q "LABH2 PASS: PCIe transaction model verified" /tmp/labh2_sim.log; then
    echo ""
    echo "      ✓ PCIe transaction model verified"
else
    echo "      ✗ Final verification message missing"
    exit 1
fi
echo ""

# Step 4: Check VCD file
echo "[4/4] Checking waveform file..."
if [ -f build/labh2.vcd ]; then
    SIZE=$(stat -f%z build/labh2.vcd 2>/dev/null || stat -c%s build/labh2.vcd 2>/dev/null)
    echo "      ✓ VCD file generated: build/labh2.vcd ($SIZE bytes)"
else
    echo "      ✗ VCD file not found"
    exit 1
fi
echo ""

echo "==================================================="
echo "✓ ALL VERIFICATION CHECKS PASSED"
echo "==================================================="
echo ""

# Offer to open GTKWave
if [ "$1" = "--wave" ]; then
    if command -v gtkwave >/dev/null 2>&1; then
        echo "Opening GTKWave with pre-configured signals..."
        if [ -f labh2_verification.gtkw ]; then
            gtkwave build/labh2.vcd labh2_verification.gtkw &
        else
            gtkwave build/labh2.vcd &
        fi
    else
        echo "GTKWave not found. Install with: sudo apt install gtkwave"
    fi
else
    echo "To view waveforms, run:"
    echo "  gtkwave build/labh2.vcd labh2_verification.gtkw"
    echo ""
    echo "Or re-run with: ./verify.sh --wave"
fi
echo ""
