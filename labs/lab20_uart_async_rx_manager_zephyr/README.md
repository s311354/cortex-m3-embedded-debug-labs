# Lab 20: UART Async RX Manager with Zephyr RTOS

## Overview

This lab demonstrates the core design of **asynchronous UART reception** using the Zephyr RTOS on ARM Cortex-M3. Unlike earlier labs that use bare-metal polling (Lab 09) or direct interrupt-driven reception (Labs 10-11), this lab implements an educational asynchronous buffer manager with two reusable driver buffers, event callbacks, and a Zephyr ring buffer.

The lab shows how a hardware-specific interrupt handler can implement Zephyr's UART asynchronous API while keeping the application behind a standard device interface. It is intentionally limited: RX uses interrupts rather than DMA, only `SYS_FOREVER_US` is supported, and asynchronous TX is not implemented.

Zephyr presents its asynchronous UART API primarily as a DMA-oriented interface. This lab implements the same callback and buffer-ownership contract with one interrupt per received byte so the state transitions remain visible in a debugger. It teaches the API semantics, not the CPU-efficiency characteristics of a typical DMA-backed asynchronous UART driver.

## Learning Objectives

- Implement Zephyr UART Async API with interrupt-driven reception
- Master asynchronous buffer ownership and transfer patterns
- Design event-driven callback architectures for device drivers
- Understand how a second receive buffer avoids a gap during buffer handoff
- Integrate custom device drivers into Zephyr's device model
- Use Zephyr device tree overlays to customize hardware bindings
- Implement ring buffers for producer-consumer patterns in RTOS context
- Apply critical sections with IRQ locking to obtain consistent shared-state snapshots
- Debug multi-layer driver stacks with event counters and state observability
- Use software-triggered interrupts via NVIC for state machine coordination

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│  Application Layer (main.c + rx_manager)                 │
│  - RX Manager state machine                              │
│  - Ring buffer for application data                      │
│  - Consumer reads at application pace                    │
└────────────────┬────────────────────────────────────────┘
                 │ rx_manager_init()
                 │ rx_manager_event() callbacks
                 │ rx_manager_read()
                 ▼
┌─────────────────────────────────────────────────────────┐
│  Zephyr UART Async Driver (uart_cmsdk_async.c)          │
│  - Buffer ownership state machine                        │
│  - Two-buffer handoff (2 x 8-byte driver buffers)       │
│  - Event generation and callback dispatch                │
│  - Hardware interrupt handling                           │
└────────────────┬────────────────────────────────────────┘
                 │ MMIO register access
                 │ NVIC interrupt control
                 ▼
┌─────────────────────────────────────────────────────────┐
│  Hardware Layer (CMSDK UART + Cortex-M3 NVIC)           │
│  - UART registers: DATA, STATE, CTRL, INTCLEAR          │
│  - RX interrupt generation on received byte              │
│  - NVIC IRQ routing and pending control                  │
└─────────────────────────────────────────────────────────┘
```

## Key Architectural Concepts

### 1. Asynchronous Buffer Ownership Model

The UART API uses an **ownership handoff** between the RX manager and the driver. The buffers are allocated by the manager and temporarily owned by the driver; no separate driver-side buffer is used. This is not an end-to-end zero-copy data path: the ISR stores bytes read from the UART register into the current driver buffer, and the manager later copies reported spans into its ring buffer.

```c
struct rx_manager {
    // Application-allocated buffers temporarily owned and filled by the driver
    uint8_t driver_buf[RX_DRIVER_COUNT][RX_DRIVER_SIZE];  // 2 x 8 bytes
    bool driver_owned[RX_DRIVER_COUNT];

    // Ring buffer owned by application (software reads here)
    struct ring_buf app_ring;
    uint8_t app_storage[RX_RING_SIZE];  // 64 bytes
};
```

**Buffer Lifecycle:**
1. Application provides buffer A to driver via `uart_rx_enable()`
2. The driver starts reception into buffer A
3. As reception starts on A, the driver requests buffer B via `UART_RX_BUF_REQUEST`
4. The application responds with buffer B via `uart_rx_buf_rsp()` before A fills
5. When A fills, the driver emits `UART_RX_RDY`, switches to B, and releases A via `UART_RX_BUF_RELEASED`
6. The driver immediately requests another buffer; the application can now return A for reuse

**Why Double Buffering?**
- Avoids a receive gap at the buffer boundary when the next buffer is supplied in time
- Lets the driver switch buffers without waiting for the application to allocate memory
- Allows continuous reception without polling or busy-waiting
- Is a common pattern in DMA-based and interrupt-driven I/O

Double buffering does not by itself guarantee lossless reception. Hardware overrun, failure to provide the next buffer, or overflow of the application ring can still lose data.

### 2. Zephyr UART Async API Event Model

The driver implements the standard Zephyr asynchronous UART event interface:

The RX-related members of Zephyr's `enum uart_event_type` are:

- `UART_RX_RDY`: new data is ready in a supplied buffer
- `UART_RX_BUF_REQUEST`: the driver requests the buffer after the current one
- `UART_RX_BUF_RELEASED`: a buffer is no longer owned by the driver
- `UART_RX_DISABLED`: reception has stopped and can be enabled again
- `UART_RX_STOPPED`: reception stopped because of an external event such as overrun

The same enum also contains the asynchronous TX events `UART_TX_DONE` and `UART_TX_ABORTED`.

**Event Flow:**
```
uart_rx_enable(buf_A)
    ↓
UART_RX_BUF_REQUEST  ←─── Driver requests the buffer after A
    ↓
uart_rx_buf_rsp(buf_B)
    ↓
UART_RX_RDY (buf_A, offset, len)  ←─── Data available in buf_A
    ↓
UART_RX_BUF_RELEASED (buf_A)  ←─── buf_A ownership returned
    ↓
UART_RX_BUF_REQUEST  ←─── Need buf_A again
    ↓
uart_rx_buf_rsp(buf_A)
    ↓
UART_RX_RDY (buf_B, offset, len)  ←─── buf_B became full
    ↓
UART_RX_BUF_RELEASED (buf_B)
    ↓
... cycle continues ...
```

With this lab's `SYS_FOREVER_US` timeout, `UART_RX_RDY` is generated when an 8-byte buffer fills or when reception is disabled/stopped. Merely requesting the next buffer does not make partially received data ready.

### 3. Ring Buffer Producer-Consumer Pattern

The RX manager uses a **ring buffer** to decouple interrupt timing from application processing:

```
ISR (Producer)                     Main Thread (Consumer)
    │                                      │
    ▼                                      ▼
UART_RX_RDY callback              rx_manager_read()
    │                                      │
    ├─ ring_buf_put()                     ├─ ring_buf_get()
    │  (add received bytes)               │  (remove bytes for processing)
    │                                      │
    └─ bytes_buffered++                   └─ bytes_read++
```

**Capacity:** 64 bytes ring buffer absorbs temporary bursts when application is slower than reception rate.

**Critical Sections:**
```c
unsigned int key = irq_lock();
uint32_t count = ring_buf_get(&mgr->app_ring, dst, limit);
irq_unlock(key);
```
The locks serialize the lab's ring-buffer operations and make related counters and diagnostic snapshots consistent. Zephyr's ring buffer also supports a single producer and a single consumer in separate contexts without additional locking; the explicit locking here is an educational synchronization choice and keeps the surrounding state updates atomic on this single-core target.

## Cortex-M3 Concepts Demonstrated

### 1. NVIC Software-Triggered Interrupts

The driver uses **software-generated interrupts** for state machine coordination:

```c
// Trigger interrupt to process initial buffer request
NVIC_SetPendingIRQ((IRQn_Type) config->rx_irq);

// Clear any stale pending interrupts
NVIC_ClearPendingIRQ((IRQn_Type) config->rx_irq);
```

**Use Cases:**
- Process initial `UART_RX_BUF_REQUEST` without waiting for hardware event
- Handle `uart_rx_disable()` gracefully by scheduling cleanup in ISR context
- Coordinate state transitions without hardware dependency

**Why Software IRQs?**
- Keeps this driver's start, receive, and stop event processing in one ISR context
- Defers start/stop state transitions until after the API call releases its IRQ lock
- Exercises Cortex-M3 pending-interrupt control without requiring a hardware byte event

This is one possible driver design, not Zephyr's general deferred-work mechanism. Work queues or dedicated threads are more appropriate when callback processing may block or take substantial time.

### 2. Critical Section Management with IRQ Locking

Proper synchronization between ISR and thread contexts:

```c
void rx_manager_event(struct rx_manager *mgr, const struct uart_event *evt) {
    unsigned int key = irq_lock();  // Mask regular Zephyr-managed interrupts

    switch (evt->type) {
        case UART_RX_RDY: {
            uint32_t copied = ring_buf_put(&mgr->app_ring, ...);
            mgr->bytes_buffered += copied;
            break;
        }
    }

    irq_unlock(key);  // Restore previous interrupt state
}
```

**Cortex-M3 Mechanism:**
- On this ARMv7-M target, Zephyr implements `irq_lock()` with `BASEPRI`
- NMI, faults, SVC, and configured zero-latency interrupts can still run
- Single-core atomic protection (this lab validates `!CONFIG_SMP`)
- More efficient than disabling individual peripherals

### 3. Memory-Mapped I/O with Volatile Semantics

Direct UART hardware register manipulation:

```c
struct uart_cmsdk_regs {
    volatile uint32_t data;      // 0x00 - RX/TX data register
    volatile uint32_t state;     // 0x04 - Status flags (RXBF, TXBF, RXOR)
    volatile uint32_t ctrl;      // 0x08 - Control (RXEN, TXEN, RXIRQEN)
    volatile uint32_t intclear;  // 0x0C - Interrupt acknowledge
    volatile uint32_t bauddiv;   // 0x10 - Baud rate divisor
};

BUILD_ASSERT(offsetof(struct uart_cmsdk_regs, data) == 0x00);
```

**Key Points:**
- `volatile` prevents compiler from caching register reads
- Compile-time assertions validate hardware assumptions
- Single structure maps entire peripheral register block
- Offset validation catches ABI or packing issues

### 4. Interrupt Service Routine Design

Educational ISR implementation:

```c
static void lab20_uart_isr(const void *arg) {
    // 1. Enter critical section
    unsigned int key = irq_lock();
    ++data->irq_entries;

    // 2. Clear hardware interrupt immediately
    regs->intclear = UART_CMSDK_INT_RX;

    // 3. Handle state machine
    if (data->state == LAB20_RX_STOPPING) {
        finish_rx(dev, 0);
        goto out;
    }

    // 4. Check for hardware errors
    uint32_t status = regs->state;
    if ((status & UART_CMSDK_STATE_RXOR) != 0U) {
        ++data->hardware_overruns;
        finish_rx(dev, UART_ERROR_OVERRUN);
        goto out;
    }

    // 5. Read byte from the hardware receive buffer
    if ((status & UART_CMSDK_STATE_RXBF) != 0U) {
        data->current_buf[data->current_pos++] =
            (uint8_t)(regs->data & UART_CMSDK_DATA_MASK);
        ++data->hardware_bytes;
    }

    // 6. Check if buffer full, swap buffers if needed
    if (data->current_pos >= data->current_len) {
        report_current(dev);  // Generate UART_RX_RDY event
        // ... buffer swap logic ...
    }

out:
    irq_unlock(key);
}
```

**ISR Design Characteristics:**
- Bounded processing based on small 8-byte receive buffers
- Clear interrupt early to prevent spurious re-entry
- State-driven decision making
- Explicit error detection and reporting
- Synchronous event callbacks in ISR context
- Counters for debugging and diagnostics

The callback currently copies `UART_RX_RDY` data into the application ring while still in ISR context. Callbacks therefore must not block. A production driver might defer substantial application work to a thread or work queue.

## Zephyr RTOS Integration

### 1. Device Tree and Custom Bindings

The lab extends Zephyr's hardware description system:

**Device Tree Overlay (app.overlay):**
```dts
&uart0 {
    compatible = "lab20,cmsdk-uart-async";
    status = "okay";
};
```

**Custom Binding (dts/bindings/serial/lab20,cmsdk-uart-async.yaml):**
```yaml
description: Lab20 interrupt-backed CMSDK APB UART asynchronous RX
compatible: "lab20,cmsdk-uart-async"
include: uart-controller.yaml
```

**Benefits:**
- Replaces the node's compatible binding while preserving its existing hardware properties
- Reuses the board's `reg`, `interrupts`, `clocks`, and `current-speed` values
- Demonstrates driver customization in real projects
- Clean separation of hardware description from driver logic

The lab-local `lab20` compatible works, but the current build reports an unknown devicetree vendor-prefix warning. A warning-free reusable binding should use a registered vendor prefix.

### 2. Zephyr Device Model

Standard Zephyr driver registration:

```c
DEVICE_DT_INST_DEFINE(
    0,                              // Device instance
    lab20_uart_init,                // Init function
    NULL,                           // PM device
    &lab20_uart_data_0,            // Runtime data
    &lab20_uart_config_0,          // Config data
    PRE_KERNEL_1,                  // Init level
    CONFIG_SERIAL_INIT_PRIORITY,   // Init priority
    &lab20_uart_api                // API table
);
```

**Device Lifecycle:**
1. Device tree processing generates metadata
2. `lab20_uart_init()` called during kernel boot
3. Application retrieves device: `DEVICE_DT_GET(DT_NODELABEL(uart0))`
4. API calls dispatched through function pointer table

### 3. Zephyr UART Async API Implementation

The driver implements the standard async API contract:

```c
static DEVICE_API(uart, lab20_uart_api) = {
    .poll_in      = lab20_uart_poll_in,
    .poll_out     = lab20_uart_poll_out,
    .callback_set = lab20_uart_callback_set,
    .tx           = lab20_uart_tx,
    .tx_abort     = lab20_uart_tx_abort,
    .rx_enable    = lab20_uart_rx_enable,
    .rx_buf_rsp   = lab20_uart_rx_buf_rsp,
    .rx_disable   = lab20_uart_rx_disable,
};
```

The asynchronous TX functions and polling input return `-ENOTSUP`; polling output is implemented but unused by `main()`.

**API Contract Guarantees:**
- `callback_set()` must succeed before `rx_enable()`
- `rx_enable()` fails if RX already active or no callback registered
- `rx_buf_rsp()` is valid only during active reception and in response to an outstanding buffer request
- `rx_disable()` triggers graceful shutdown with final events
- All events delivered in ISR context (callbacks must not block)

## Configuration and Build

### Project Configuration (`prj.conf`)

```kconfig
# Enable serial subsystem
CONFIG_SERIAL=y
CONFIG_LAB20_CMSDK_UART_ASYNC=y
CONFIG_UART_ASYNC_API=y
CONFIG_UART_INTERRUPT_DRIVEN=n
CONFIG_UART_USE_RUNTIME_CONFIGURE=n

# Application receive queue
CONFIG_RING_BUFFER=y

# UART0 is reserved for this lab rather than the console
CONFIG_CONSOLE=n
CONFIG_UART_CONSOLE=n
CONFIG_PRINTK=n
CONFIG_LOG=n

# Debug-friendly build
CONFIG_DEBUG=y
CONFIG_NO_OPTIMIZATIONS=y
CONFIG_ASSERT=y
```

The project requests `CONFIG_PRINTK=n`, but the recorded Zephyr build warns that the effective value is `y`. It should therefore not be treated as disabled unless the Kconfig dependency selecting it is also changed.

### Custom Driver Kconfig (Kconfig)

```kconfig
config LAB20_CMSDK_UART_ASYNC
    bool "Lab20 interrupt-backed CMSDK UART asynchronous RX"
    default y
    depends on SERIAL && CPU_CORTEX_M3 && !SMP
    depends on DT_HAS_LAB20_CMSDK_UART_ASYNC_ENABLED
    select SERIAL_HAS_DRIVER
    select SERIAL_SUPPORT_ASYNC
```

## Debugging and Observability

### 1. Debug-Friendly Breakpoint Helpers

```c
__attribute__((noinline))
void lab20_ready(void) {
    __asm__ volatile("" ::: "memory");
}

__attribute__((noinline))
void lab20_after_read(void) {
    __asm__ volatile("" ::: "memory");
}
```

**Purpose:**
- Noinline functions create **stable GDB breakpoint targets**
- Memory barrier prevents compiler reordering
- Allows breakpoint before/after critical operations

### 2. Observable Global State

```c
volatile bool g_consume_enabled = true;
volatile bool g_stop_requested;
volatile int g_init_rc;

uint8_t g_last_read[16];
size_t g_last_read_len;
uint32_t g_total_read;
uint32_t g_ring_used;
```

**GDB Session:**
```gdb
(gdb) break lab20_ready
(gdb) continue
(gdb) print g_init_rc
$1 = 0

(gdb) print g_rx_manager
$2 = {
  enabled = true,
  ready_events = 5,
  request_events = 6,
  released_events = 5,
  bytes_received = 40,
  bytes_buffered = 40,
  bytes_read = 32,
  dropped_bytes = 0
}

(gdb) x/64xb g_rx_manager.app_storage
0x200001a0: 0x48 0x65 0x6c 0x6c 0x6f 0x20 0x57 0x6f
```

### 3. Event Counters for Diagnostics

The RX manager tracks detailed statistics:

```c
struct rx_manager {
    uint32_t ready_events;      // Number of UART_RX_RDY events
    uint32_t request_events;    // Number of UART_RX_BUF_REQUEST events
    uint32_t released_events;   // Number of UART_RX_BUF_RELEASED events
    uint32_t disabled_events;   // Number of UART_RX_DISABLED events
    uint32_t stopped_events;    // Number of UART_RX_STOPPED events

    uint32_t bytes_received;    // Total bytes reported by driver
    uint32_t bytes_buffered;    // Total bytes successfully stored
    uint32_t bytes_read;        // Total bytes consumed by application
    uint32_t dropped_bytes;     // Bytes lost to ring buffer overflow
};
```

**Debugging Scenarios:**

**Normal Operation:**
```
bytes_received == bytes_buffered + dropped_bytes
dropped_bytes == 0
```

When the ring is empty at a stable observation point:

```
bytes_buffered == bytes_read
```

While data is queued, `bytes_buffered - bytes_read` is the amount not yet consumed, subject to taking a consistent snapshot of the counters and ring state.

**Ring Buffer Overflow:**
```
bytes_received > bytes_buffered
dropped_bytes > 0
```
→ Application consuming too slowly or ring buffer too small

**Missing/Rejected Next Buffer:**
```
last_error == -ENOBUFS
or uart_rx_buf_rsp() returned an error
```
→ No reusable buffer was available when the driver requested one, or the response violated the driver's state contract

During ordinary active reception, this two-buffer implementation normally has one more request than release. The relationship `request_events > released_events + 1` is therefore not a reliable starvation diagnostic.

### 4. Hardware-Level Diagnostics

The driver data structure exposes hardware counters:

```c
struct lab20_uart_data {
    uint32_t irq_entries;        // Total ISR invocations
    uint32_t hardware_bytes;     // Bytes read from UART DATA register
    uint32_t hardware_overruns;  // RXOR flag detections
};
```

## Testing the Lab

### Test Sequence

The main loop continuously reads from the ring buffer:

```c
for (;;) {
    if (g_consume_enabled) {
        size_t n = rx_manager_read(&g_rx_manager,
                                   g_last_read,
                                   sizeof(g_last_read));
        if (n != 0U) {
            g_last_read_len = n;
            g_total_read += (uint32_t) n;
            lab20_after_read();  // Breakpoint helper
        }
    }

    // Expose ring buffer usage for debugging
    unsigned int key = irq_lock();
    g_ring_used = ring_buf_size_get(&g_rx_manager.app_ring);
    irq_unlock(key);

    k_sleep(K_MSEC(20));  // Yield to RTOS
}
```

### Send Test Data

Run QEMU in one terminal:

```bash
make run
# QEMU prints a line similar to:
# char device redirected to /dev/pts/5
```

In another terminal, pass that PTY path to the supplied helper:

```bash
./scripts/send_bytes.py --port /dev/pts/5 --text "12345678"

# Hexadecimal input and a custom inter-byte delay
./scripts/send_bytes.py --port /dev/pts/5 --hex "41 42 43 44 45 46 47 48" --interval 0.01
```

The helper sends one finite payload; it has no `--stream` or `--rate` option.

Use a multiple of the 8-byte driver-buffer size when testing normal `UART_RX_RDY` events. For example, `"Hello, Lab20!"` is 13 bytes: the first eight bytes are reported when buffer A fills, while the remaining five stay in buffer B until three more bytes arrive or RX is disabled. This happens because the lab uses `SYS_FOREVER_US` and does not implement inactivity timeouts.

### Expected Behavior

With properly working async reception:

1. **Initial startup:** `g_init_rc == 0`
2. **Buffer requests:** Driver emits `UART_RX_BUF_REQUEST` immediately
3. **Data reception:** Each byte triggers the ISR and is stored in the current 8-byte driver buffer
4. **Event generation:** `UART_RX_RDY` when a buffer fills or when reception is disabled/stopped with pending data
5. **Buffer swap:** Seamless transition between driver_buf[0] and driver_buf[1]
6. **Ring copy:** The `UART_RX_RDY` callback copies the reported span into the application ring
7. **Application read:** The main loop drains the ring buffer
8. **No ring overflow:** `bytes_received == bytes_buffered` and `dropped_bytes == 0`

These counters cover bytes reported by the UART driver. They do not prove that no byte was lost at the hardware interface; also check `stopped_events`, `last_stop_reason`, and the driver's `hardware_overruns` counter.

## Comparison with Previous Labs

| Feature | Lab 09 (Polling) | Lab 10 (Interrupt) | Lab 11 (Ring Buffer) | **Lab 20 (Async + RTOS)** |
|---------|------------------|-------------------|----------------------|---------------------------|
| **Reception Model** | Blocking poll | Simple ISR echo | ISR → Ring → Main | Async buffer ownership |
| **Buffering** | None | None | 64-byte ring | Double buffer + 64-byte ring |
| **Burst Handling** | N/A | No queue | Ring buffer absorbs bounded bursts | Two-buffer handoff + bounded ring |
| **RTOS Integration** | None | None | None | **Full Zephyr integration** |
| **Event Model** | None | None | None | **Multi-event callback system** |
| **API Abstraction** | Direct UART | Direct UART | Direct UART | **Standard Zephyr UART application API** |
| **Error Handling** | None | Basic | Basic | **RX overrun stop/reporting** |
| **Debug Observability** | Minimal | Basic | Basic | **Event and byte counters** |

## Key Takeaways

- **Asynchronous I/O patterns** avoid busy polling, although this teaching driver still services every received byte in an ISR
- **Two-buffer handoff** avoids a receive gap when the next buffer is supplied in time
- **Event callbacks** provide a standard driver/application interface, while executing synchronously in this ISR
- **Ring buffers** absorb temporary mismatches between producer and consumer rates
- **IRQ locking** provides critical sections for shared data in single-core systems
- **Software-triggered interrupts** coordinate state machine transitions safely
- **Zephyr device model** gives the application a portable API over hardware-specific driver code
- **Observable state** and event counters simplify debugging complex async systems
- **Buffer ownership tracking** prevents the manager from reusing buffers still owned by the driver
- **Ordered shutdown events** return both supplied buffers before RX can be enabled again

## References

- [Zephyr UART Async API Documentation](https://docs.zephyrproject.org/latest/hardware/peripherals/uart.html#asynchronous-uart-api)
- [ARM Cortex-M3 Technical Reference Manual](https://developer.arm.com/documentation/ddi0337/)
- [ARMv7-M Architecture Reference Manual](https://developer.arm.com/documentation/ddi0403/)
- [ARM MPS2+ AN385 Technical Reference Manual](https://developer.arm.com/documentation/dai0385/)
- [Zephyr Device Driver Model](https://docs.zephyrproject.org/latest/kernel/drivers/index.html)
- [CMSIS-Core NVIC Functions](https://arm-software.github.io/CMSIS_6/main/Core/group__NVIC__gr.html)

