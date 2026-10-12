# Lab 23: Flash Boot Event Logger (Zephyr)

## Overview

This lab demonstrates Zephyr's flash driver API and recovery of an append-only log on the ARM Cortex-M3 MPS2/AN385 QEMU target. The application writes two fixed-size records to Zephyr's simulated flash device, initializes a second `boot_log` object over the same device, and recovers the records by scanning the partition.

The simulated flash is backed by RAM on this target. Its contents remain available while the current QEMU execution is running, but they are cleared when the flash driver is initialized during a new boot. Consequently, this lab demonstrates recovery after reinitializing the logger within one boot; it does **not** demonstrate persistence across a CPU reset, QEMU restart, power cycle, or firmware update.

## Learning Objectives

- Use Zephyr's flash driver API and device model
- Configure fixed flash partitions with devicetree
- Respect device-specific write-block and erase-value constraints
- Implement and inspect a fixed-record append-only log
- Reconstruct in-memory log state by scanning simulated flash
- Propagate flash-driver errors and detect erased gaps
- Debug flash API operations and simulator storage with GDB
- Understand the limitations of a simple log without commit markers or checksums

## Platform and Flash Model

The lab builds for `mps2/an385`, an ARM Cortex-M3 QEMU target. It uses Zephyr's `zephyr,sim-flash` driver rather than a hardware NOR flash controller.

For this board, the simulated device has these properties:

- Total size: `0x8000` bytes (32 KiB)
- Erase-block size: `0x400` bytes (1 KiB)
- Write-block size: 4 bytes
- Erase value: `0x00`
- Explicit erase behavior enabled
- Writes to non-erased program units rejected

Because this instance's erase value is `0x00`, erased bytes read as zero. With explicit erase behavior enabled, programming changes bits from 0 toward 1. This polarity is the inverse of the erased-`0xFF` behavior commonly associated with NOR flash.

The application does not call `flash_erase()`. It relies on the simulator being cleared to its erase value during device initialization and appends only to unused slots.

### Logical Offsets Versus CPU Addresses

Offsets passed to `flash_read()` and `flash_write()` are offsets within the simulated flash device. They are not CPU memory addresses. For example, partition offset `0x7000` must not be inspected in GDB with `x/32xb 0x7000`; that CPU address belongs to the application image on this target.

The simulator stores its contents in the SRAM array `mock_flash_0`. GDB examples later in this document use expressions such as `&mock_flash_0[0x7000]` so they remain independent of the array's link address.

## Flash Partitioning

The devicetree overlay (`app.overlay`) changes the existing storage partition and adds a boot-log partition:

```text
Storage partition:  0x00000000 - 0x00006FFF (28 KiB)
Boot-log partition: 0x00007000 - 0x00007FFF (4 KiB)
```

Each boot-log slot is exactly 16 bytes (`BOOT_LOG_RECORD_SIZE`), so the 4 KiB partition contains 256 slots.

## Boot Log Data Structures

### `struct boot_log`

```c
struct boot_log {
    const struct device *dev; /* Zephyr flash device */
    off_t base;               /* Logical partition offset: 0x7000 */
    size_t region_size;       /* Partition size: 0x1000 */
    size_t next_slot;         /* Index of the next erased slot */
    uint8_t erase_value;      /* 0x00 on MPS2/AN385 */
    bool initialized;         /* Whether this RAM object may be used */
};
```

The `initialized` member is an in-memory validity guard. It does not make a flash write atomic and does not survive a reset.

### Record Format

The demonstration uses opaque 16-byte records:

```text
Record A: 42 4F 4F 54 01 00 00 01  11 22 33 44 55 66 77 88
          |-- "BOOT" --| metadata  |--------- payload --------|

Record B: 42 4F 4F 54 02 00 00 02  99 AA BB CC DD EE F0 0F
          |-- "BOOT" --| metadata  |--------- payload --------|
```

The logger does not parse or validate these fields. It only distinguishes a completely erased slot from a slot containing at least one programmed byte.

## API Functions

### `boot_log_init()`

`boot_log_init()` clears the supplied RAM object, validates its configuration, and scans the complete region.

Validation includes:

1. Checking the log and device pointers
2. Checking that the base is non-negative
3. Requiring at least one complete 16-byte slot
4. Requiring the region size to be a multiple of 16 bytes
5. Rejecting an `off_t` range overflow
6. Checking that the device is ready
7. Requiring the base and record size to meet the device's write-block constraints
8. Reading the device's erase value

During recovery, slots must form one contiguous non-erased prefix followed by erased slots. The function scans every slot so that it can detect a non-erased slot after an erased one. On success, `next_slot` is the length of the contiguous occupied prefix; for a full partition it equals the capacity.

Directly returned errors include:

- `-EINVAL`: invalid pointer, range, size, or overflow
- `-ENODEV`: device not ready
- `-ENOTSUP`: incompatible write-block properties or unavailable flash parameters
- `-EILSEQ`: a completely erased slot is followed by a non-erased slot

Errors returned by `flash_read()` are propagated unchanged.

Gap detection is intentionally limited. A partially programmed or otherwise corrupted slot is considered occupied as long as it is not entirely equal to the erase value.

### `boot_log_append()`

`boot_log_append()` performs this sequence:

1. Validate the object, record pointer, and device state
2. Reject a record consisting entirely of the erase value
3. Check that the log has capacity
4. Read the target slot and require it to be erased
5. Set the RAM object's `initialized` flag to `false`
6. Write the 16-byte record
7. Read the slot back and compare all 16 bytes
8. Increment `next_slot` and restore `initialized` on success

Directly returned errors include:

- `-EINVAL`: invalid/uninitialized object, null record, or all-erased record
- `-ENODEV`: device not ready
- `-ENOSPC`: no slot remains
- `-EEXIST`: the expected target slot is not erased
- `-EIO`: read-back data differs from the requested record

Flash read and write errors are propagated unchanged. After `-EEXIST` or an error occurring after the flag is cleared, the object remains uninitialized and must be reconstructed with `boot_log_init()` before reuse.

The read-back comparison detects an immediate mismatch. It is not a transaction commit protocol and cannot guarantee recovery from power loss during a write.

### `boot_log_read_last()`

`boot_log_read_last()` reads slot `next_slot - 1`.

- It returns `-EINVAL` for an invalid or uninitialized object or null output buffer.
- It returns `-ENODEV` if the device is not ready.
- It returns `-ENOENT` when `next_slot == 0`.
- It returns `-EIO` if the selected slot is completely erased.
- It propagates other `flash_read()` errors unchanged.

The function does not check magic bytes, a sequence number, or a checksum.

## Configuration Files

`CONFIG_FLASH_SIMULATOR_EXPLICIT_ERASE=y` and `CONFIG_FLASH_SIMULATOR_DOUBLE_WRITES=n` cause the simulator to reject a write when a target 4-byte program unit is not entirely erased.

### `app.overlay`

```dts
&storage_partition {
    reg = <0x00000000 0x7000>;
};

&flash_sim0 {
    partitions {
        bootlog_partition: partition@7000 {
            label = "bootlog_partition";
            reg = <0x00007000 0x1000>;
        };
    };
};
```

These `reg` values are logical offsets and sizes within `flash_sim0`.

## GDB Debugging Session

### Stop at Completion

The simplest way to inspect final results is to stop when `halt_lab()` is entered:

```gdb
(gdb) break halt_lab
(gdb) continue
(gdb) print g_lab23_stage
$1 = LAB23_STAGE_DONE
```

To observe each test boundary instead:

```gdb
(gdb) break lab23_checkpoint
(gdb) continue
```

Repeated `continue` commands advance to later checkpoints.

### Inspect Results

```gdb
(gdb) print g_init_rc
(gdb) print g_empty_rc
(gdb) print g_append_a_rc
(gdb) print g_append_b_rc
(gdb) print g_recover_rc
(gdb) print g_read_last_rc
(gdb) print g_compare_rc
```

### Examine Simulated Flash

Use the simulator's backing array plus the logical flash offset:

```gdb
# First 256 bytes of the boot-log partition
(gdb) x/256xb &mock_flash_0[0x7000]

# First two records
(gdb) x/32xb &mock_flash_0[0x7000]
```

After successful completion, the first two records are:

```text
mock_flash_0 + 0x7000: 42 4F 4F 54 01 00 00 01  11 22 33 44 55 66 77 88
mock_flash_0 + 0x7010: 42 4F 4F 54 02 00 00 02  99 AA BB CC DD EE F0 0F
```

Do not use `x/32xb 0x7000`; that examines CPU code memory, not simulator storage.

### Inspect the Log Objects

After the lab reaches `LAB23_STAGE_DONE`:

```gdb
(gdb) print g_log
$2 = {
  dev = 0x...,
  base = 28672,
  region_size = 4096,
  next_slot = 2,
  erase_value = 0 '\000',
  initialized = true
}

(gdb) print g_recovered_log.next_slot
$3 = 2
```

Before the append operations, `g_log.next_slot` is zero on a fresh execution.

### Examine Recovered Data

```gdb
(gdb) x/16xb g_last_record
```

Expected bytes:

```text
42 4F 4F 54 02 00 00 02  99 AA BB CC DD EE F0 0F
```

### Step Through an Append

```gdb
(gdb) break boot_log.c:boot_log_append
(gdb) continue
(gdb) next
```

Once `offset` is in scope:

```gdb
(gdb) print/x offset
(gdb) x/16xb record
(gdb) x/16xb &mock_flash_0[offset]
```

`offset` is a logical simulator offset, so it must be used as an index into `mock_flash_0` for direct GDB inspection.

## Further Reading

- [Zephyr Flash Driver API](https://docs.zephyrproject.org/latest/hardware/peripherals/flash.html)
- [Zephyr Flash API Reference](https://docs.zephyrproject.org/latest/doxygen/html/group__flash__interface.html)
- [Zephyr Devicetree Guide](https://docs.zephyrproject.org/latest/build/dts/index.html)
- [Fixed Partitions Binding](https://docs.zephyrproject.org/latest/build/dts/api/bindings/mtd/fixed-partitions.html)
- [Simulated Flash Binding](https://docs.zephyrproject.org/latest/build/dts/api/bindings/flash_controller/zephyr%2Csim-flash.html)
- Arm Cortex-M3 Technical Reference Manual
- Armv7-M Architecture Reference Manual
