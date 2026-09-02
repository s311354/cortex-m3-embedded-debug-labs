# Lab H1 Hardware Verification SOP
## GTKWave Waveform Analysis for AHB PCIe Host Bridge

**Tool Chain**: Icarus Verilog 11.0 + GTKWave

---

## Overview

This SOP covers **hardware verification** of the `labh1_ahb_pcie_host_bridge` RTL module using:
- **Icarus Verilog** for simulation
- **GTKWave** for waveform visualization and analysis
- **Self-checking testbench** for automated verification

The verification confirms the AHB-Lite slave interface correctly handles configuration read transactions to a simulated PCIe endpoint.

---

### Source Files Required

| File | Type | Description |
|------|------|-------------|
| `rtl/labh1_ahb_pcie_host_bridge.v` | RTL | Main AHB slave + FSM |
| `rtl/labh1_pcie_backend_stub.v` | RTL | Simulated PCIe endpoint |
| `rtl/m3ds_pcie_host_wrapper.v` | RTL | Integration wrapper |
| `tb/tb_labh1.v` | Testbench | Self-checking testbench |

---

## Verification Flow

### Step 1: RTL Compilation

```bash
# Compile RTL + testbench with Icarus Verilog
iverilog -g2012 -Wall \
    -o build/labh1_sim \
    rtl/m3ds_pcie_host_wrapper.v \
    rtl/labh1_ahb_pcie_host_bridge.v \
    rtl/labh1_pcie_backend_stub.v \
    tb/tb_labh1.v
```

**Expected Result**: 
- Output file: `build/labh1_sim`
- No compilation errors or warnings
- Exit status: 0

**Common Issues**:
- **Syntax errors**: Check Verilog-2012 compliance
- **Missing files**: Verify file paths are correct
- **Module not found**: Ensure all dependencies included

---

### Step 2: Run Simulation

```bash
# Execute simulation
vvp build/labh1_sim
```

**Expected Output**:
```
VCD info: dumpfile build/labh1.vcd opened for output.
LABH1 PASS: CF_READ 00:01.0 -> 56781234
```

**Generated Files**:
- `build/labh1.vcd` - Value Change Dump waveform file

**Pass Criteria**:
- ✅ Message shows `LABH1 PASS`
- ✅ Config read returns `0x56781234` (Device=0x5678, Vendor=0x1234)
- ✅ No `LABH1 FAIL` messages

**Failure Scenarios**:
| Error Message | Root Cause |
|--------------|------------|
| `FAIL: VERSION=xxxxxxxx` | Hardware version mismatch |
| `FAIL: timeout status=xx` | STATUS.DONE bit never set |
| `FAIL: backend error` | STATUS.ERROR bit set |
| `FAIL: CFG_RDATA=xxxxxxxx` | Wrong device ID returned |

---

### Step 3: Launch GTKWave

```bash
# Open waveform viewer
gtkwave build/labh1.vcd &
```
---

## GTKWave Analysis Procedure

### Critical Signal Groups

#### Group 1: Clock and Reset

**Purpose**: Verify timing reference and reset sequence

```
Signals to add:
- tb_labh1.HCLK
- tb_labh1.HRESETn
```

**Verification Checks**:
- ✅ HCLK toggles at 10ns period (100 MHz)
- ✅ HRESETn asserts low for ~40ns
- ✅ HRESETn goes high before transactions start

**GTKWave Tips**:
- Right-click signal → Data Format → **Binary** (for HRESETn)
- Right-click signal → Data Format → **Analog** (for HCLK if desired)

---

#### Group 2: AHB Interface Signals

**Purpose**: Verify Cortex-M3 bus transactions

```
Signals to add:
- tb_labh1.HSEL          [Select]
- tb_labh1.HADDR         [Address]
- tb_labh1.HTRANS        [Transfer Type]
- tb_labh1.HWRITE        [Read/Write]
- tb_labh1.HSIZE         [Size]
- tb_labh1.HWDATA        [Write Data]
- tb_labh1.HRDATA        [Read Data]
- tb_labh1.HREADY        [Previous Ready]
- tb_labh1.HREADYOUT     [Current Ready]
- tb_labh1.HRESP         [Response]
```

**Verification Sequence**:

Find the **VERSION read** (first transaction):
1. Locate where `HSEL=1` and `HTRANS=2'b10` (NONSEQ)
2. Check `HADDR = 0xA0000000` (REG_VERSION)
3. Check `HWRITE = 0` (read operation)
4. **Next cycle**: Check `HRDATA = 0x00010000`

Find the **CONTROL write** (enable bridge):
1. Locate `HSEL=1`, `HTRANS=2'b10`, `HADDR=0xA0000004`
2. Check `HWRITE = 1` (write operation)
3. Check `HWDATA = 0x00000001` (ENABLE bit)

Find the **CFG_COMMAND write** (doorbell):
1. Locate `HADDR=0xA0000020` (REG_CFG_COMMAND)
2. Check `HWDATA = 0x00000001` (CMD_CFG_READ)

**AHB Protocol Checks**:
- ✅ Address phase and data phase are **1 cycle apart**
- ✅ HREADYOUT always returns `1` (zero wait states)
- ✅ HRESP always `0` (no errors)

**GTKWave Tips**:
- Right-click HADDR/HRDATA/HWDATA → Data Format → **Hexadecimal**
- Right-click HTRANS → Data Format → **Binary** (easier to see 2'b10)

---

#### Group 3: Internal Register State

**Purpose**: Verify register updates and FSM state

```
Signals to add:
- tb_labh1.u_bridge.control
- tb_labh1.u_bridge.cfg_bdf
- tb_labh1.u_bridge.cfg_reg
- tb_labh1.u_bridge.cfg_command
- tb_labh1.u_bridge.cfg_rdata
- tb_labh1.u_bridge.busy
- tb_labh1.u_bridge.done
- tb_labh1.u_bridge.error_status
- tb_labh1.u_bridge.state
```

**Verification Checks**:

After CONTROL write:
- ✅ `control = 0x00000001`

After CFG_BDF write:
- ✅ `cfg_bdf = 0x00000800` (Bus=0, Device=1, Function=0)

After CFG_REG write:
- ✅ `cfg_reg = 0x00000000` (config DWORD 0)

After CFG_COMMAND write:
- ✅ `cfg_command = 0x00000001`
- ✅ `busy = 1` (transaction in progress)
- ✅ `done = 0` (cleared on new command)
- ✅ `state = 2'd1` (STATE_ISSUE)

After completion:
- ✅ `busy = 0`
- ✅ `done = 1`
- ✅ `error_status = 0x00000000`
- ✅ `cfg_rdata = 0x56781234`
- ✅ `state = 2'd0` (STATE_IDLE)

**GTKWave Tips**:
- Right-click state → Data Format → **Decimal** (easier to read 0/1/2)
- Add divider: Edit → Insert Blank → Add label "=== Register State ==="

---

#### Group 4: Backend Request Interface

**Purpose**: Verify PCIe backend communication

```
Signals to add:
- tb_labh1.req_valid
- tb_labh1.req_ready
- tb_labh1.req_type
- tb_labh1.req_bdf
- tb_labh1.req_reg
- tb_labh1.req_wdata
```

**Verification Checks**:

When `state = STATE_ISSUE`:
- ✅ `req_valid = 1`
- ✅ `req_type = 2'd0` (REQ_CFG_READ)
- ✅ `req_bdf = 0x00000800`
- ✅ `req_reg = 0x000` (10-bit value)

When backend accepts:
- ✅ `req_ready = 1` (same cycle or later)
- ✅ State transitions to `STATE_WAIT_CPL` (2'd2)

**Handshake Timing**:
- Request accepted when: `req_valid && req_ready`
- May take multiple cycles if backend busy

---

#### Group 5: Backend Completion Interface

**Purpose**: Verify PCIe response handling

```
Signals to add:
- tb_labh1.cpl_valid
- tb_labh1.cpl_ready
- tb_labh1.cpl_status
- tb_labh1.cpl_rdata
```

**Verification Checks**:

When `state = STATE_WAIT_CPL`:
- ✅ `cpl_ready = 1` (bridge ready to receive)

When backend completes:
- ✅ `cpl_valid = 1`
- ✅ `cpl_status = 2'd0` (success)
- ✅ `cpl_rdata = 0x56781234`

After completion handshake:
- ✅ Bridge updates `cfg_rdata = 0x56781234`
- ✅ Bridge sets `done = 1`
- ✅ State returns to `STATE_IDLE`

**Completion Timing**:
- Completion accepted when: `cpl_valid && cpl_ready`
- Backend stub adds 2-cycle artificial delay

---

#### Group 6: Backend Stub Internals (Optional)

**Purpose**: Understand backend behavior

```
Signals to add:
- tb_labh1.u_backend.pending
- tb_labh1.u_backend.delay_count
- tb_labh1.u_backend.saved_type
- tb_labh1.u_backend.saved_bdf
- tb_labh1.u_backend.saved_reg
```

**Observable Behavior**:
1. `req_valid && req_ready` → `pending = 1`
2. `delay_count` counts down: 2 → 1 → 0
3. When `delay_count = 0` → `cpl_valid = 1`
4. After handshake → `pending = 0`

---

### Advanced Analysis

#### Measuring Transaction Latency

**Goal**: Measure time from command write to completion

1. **Find CMD write time**: 
   - Locate where `HADDR=0xA0000020` and `HWRITE=1`
   - Note timestamp (e.g., `T1 = 85ns`)

2. **Find STATUS read with DONE**:
   - Locate where testbench reads `REG_STATUS`
   - Find first read where `HRDATA[1]=1` (DONE bit)
   - Note timestamp (e.g., `T2 = 155ns`)

3. **Calculate latency**: `T2 - T1 = 70ns = 7 clock cycles`

**GTKWave Tip**: 
- Place cursor on first event → Press "T" to mark
- Place cursor on second event → View time difference in bottom status bar

---

#### Verifying FSM State Transitions

**Expected State Sequence**:
```
IDLE (0) → ISSUE (1) → WAIT_CPL (2) → IDLE (0)
```

**GTKWave View**:
1. Add signal: `tb_labh1.u_bridge.state`
2. Right-click → Data Format → Decimal
3. Zoom to show full transaction
4. Verify state changes occur on HCLK rising edges
5. Count cycles in each state:
   - ISSUE: 1 cycle (if `req_ready=1`)
   - WAIT_CPL: 2-3 cycles (backend delay)

---

#### Verifying AHB Two-Phase Protocol

**Address Phase**:
- `HSEL=1`, `HTRANS=2'b10`, `HADDR=X`, `HWRITE=W`
- Sampled on rising edge of HCLK

**Data Phase** (next cycle):
- If write: `HWDATA=D` valid
- If read: `HRDATA=D` valid

**GTKWave Measurement**:
1. Find address phase (e.g., at 85ns)
2. Verify data phase at 95ns (next clock rising edge)
3. Confirm 1 cycle (10ns) separation

---

## Signal Naming Convention Reference

| Signal Prefix | Domain | Example |
|---------------|--------|---------|
| `H*` | AHB-Lite | `HADDR`, `HWRITE` |
| `req_*` | Backend Request | `req_valid`, `req_bdf` |
| `cpl_*` | Backend Completion | `cpl_valid`, `cpl_rdata` |
| `cfg_*` | Config Registers | `cfg_bdf`, `cfg_rdata` |
| `*_r` | Internal Register | `req_type_r`, `req_bdf_r` |

---

## Pass/Fail Criteria Summary

### ✅ PASS Criteria

1. **Testbench Output**: `LABH1 PASS: CF_READ 00:01.0 -> 56781234`
2. **AHB Protocol**: All transactions follow 2-phase timing
3. **Register Updates**: All writes successfully update internal registers
4. **FSM Behavior**: States transition correctly (IDLE→ISSUE→WAIT_CPL→IDLE)
5. **Backend Handshake**: Request/completion use valid/ready correctly
6. **Data Integrity**: Final `cfg_rdata = 0x56781234`
7. **Error Flags**: `error_status = 0x00000000` at end

### ❌ FAIL Scenarios

| Symptom | Likely Cause | GTKWave Check |
|---------|--------------|---------------|
| Timeout message | DONE never set | Check `u_bridge.done` signal |
| Backend error | Bad BDF/REG | Check `cpl_status != 0` |
| Wrong data | Backend stub bug | Check `u_backend.saved_*` |
| Hung FSM | Missing transition | Check `u_bridge.state` stuck |
| AHB error | Protocol violation | Check `HRESP = 1` |

---

## GTKWave Productivity Tips

### Saving Signal Configuration

1. After adding signals: File → Write Save File
2. Save as: `lab1.gtkw`
3. Next session: `gtkwave build/labh1.vcd lab1.gtkw`

### Signal Grouping

1. Select multiple signals in wave view
2. Right-click → Group signals
3. Name group (e.g., "AHB Bus")
4. Collapse/expand as needed

### Search for Events

1. Search → Signal Search → Pattern Search
2. Example: Find when `HADDR = 0xA0000020`
3. Example: Find rising edges of `req_valid`

---

## References

- **Lab README**: `README.md` - Hardware architecture overview
- **RTL Source**: `rtl/labh1_ahb_pcie_host_bridge.v` - Implementation
- **Testbench**: `tb/tb_labh1.v` - Verification code
- **AHB Spec**: ARM IHI 0033B - AMBA AHB-Lite Protocol
- **GTKWave Manual**: http://gtkwave.sourceforge.net/gtkwave.pdf
