#!/bin/bash
# Quick verification script for Lab H1
# Usage: ./verify.sh [--wave]

set -e

echo "==================================================="
echo "Lab H1 Hardware Verification Script"
echo "==================================================="
echo ""

# Step 1: Clean previous build
echo "[1/4] Cleaning previous build..."
make sim-clean 2>/dev/null || true
echo "      ✓ Clean complete"
echo ""

# Step 2: Run simulation
echo "[2/4] Running RTL simulation..."
if make sim 2>&1 | tee /tmp/labh1_sim.log; then
    echo "      ✓ Simulation complete"
else
    echo "      ✗ Simulation FAILED"
    exit 1
fi
echo ""

# Step 3: Check results
echo "[3/4] Verifying results..."
if grep -q "LABH1 PASS" /tmp/labh1_sim.log; then
    RESULT=$(grep "LABH1 PASS" /tmp/labh1_sim.log)
    echo "      ✓ PASS: $RESULT"
    
    # Extract and validate the value
    if echo "$RESULT" | grep -q "56781234"; then
        echo "      ✓ Correct device ID (0x56781234)"
        echo "        - Vendor: 0x1234"
        echo "        - Device: 0x5678"
    else
        echo "      ✗ Wrong device ID returned"
        exit 1
    fi
else
    echo "      ✗ FAIL: Test did not pass"
    grep "LABH1 FAIL" /tmp/labh1_sim.log || echo "Unknown error"
    exit 1
fi
echo ""

# Step 4: Check VCD file
echo "[4/4] Checking waveform file..."
if [ -f build/labh1.vcd ]; then
    SIZE=$(stat -f%z build/labh1.vcd 2>/dev/null || stat -c%s build/labh1.vcd 2>/dev/null)
    echo "      ✓ VCD file generated: build/labh1.vcd ($SIZE bytes)"
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
        if [ -f labh1_verification.gtkw ]; then
            gtkwave build/labh1.vcd labh1_verification.gtkw &
        else
            gtkwave build/labh1.vcd &
        fi
    else
        echo "GTKWave not found. Install with: sudo apt install gtkwave"
    fi
else
    echo "To view waveforms, run:"
    echo "  gtkwave build/labh1.vcd labh1_verification.gtkw"
    echo ""
    echo "Or re-run with: ./verify.sh --wave"
fi
echo ""
