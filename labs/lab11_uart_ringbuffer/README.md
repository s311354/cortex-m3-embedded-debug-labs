# Lab 11: UART Ring Buffer

## Overview

This lab demonstrates interrupt-driven UART reception with a software ring buffer for asynchronous data handling. The UART RX ISR acts as the producer, placing received bytes into a fixed-size ring buffer, while the main loop acts as the consumer and processes buffered bytes independently.

Reception is interrupt-driven, while transmission still uses polling.

## Learning Objectives

- Implement a circular ring buffer data structure
- Understand producer-consumer patterns in embedded systems
- Learn proper use of `volatile` keyword for shared data
- Practice interrupt-driven I/O with buffering
- Understand the importance of keeping ISRs short and fast
- Understand atomicity and ownership requirements for ISR/main shared state
- Debug concurrent data access patterns

## Key Concepts

### Ring Buffer (Circular Buffer)

A ring buffer is a fixed-size FIFO (First-In-First-Out) queue that wraps around when it reaches the end:

```
Empty:     head == tail
  [ ][ ][ ][ ][ ][ ][ ][ ]
   ^
   head/tail

After push('A','B','C'):
  [A][B][C][ ][ ][ ][ ][ ]
   ^        ^
   tail     head

After pop() -> 'A':
  [A][B][C][ ][ ][ ][ ][ ]
      ^     ^
      tail  head

Full:      (head + 1) % SIZE == tail
  [A][B][C][D][E][F][G][ ]
                        ^  ^
                        tail head

(7 usable entries in an 8-slot ring)

```
This implementation reserves one slot to distinguish full from empty. Therefore, with `RB_SIZE == 64`, the usable capacity is 63 bytes.

### Producer-Consumer Architecture

**Producer (UART0_Handler - ISR)**
- Runs in interrupt context
- Triggered on each received character
- Pushes data into ring buffer
- Must be fast to minimize interrupt latency

**Consumer (main loop)**
- Runs in thread mode
- Polls ring buffer for available data
- Pops and echoes characters via UART TX
- Can be delayed temporarily without immediately blocking reception, as long as the ring buffer does not become full

### Volatile and Shared ISR State

```c
typedef struct {
    volatile char data[RB_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuffer_t;
```
The ISR and main loop access the same ring-buffer state asynchronously:
- the ISR is the only writer of `head`
- the main loop is the only writer of `tail`
- each context reads the index owend by the other context

In this bare-metal single-core example, the shared fields are declared `volatile` so the compiler performs the required accesses rather than assuming that values cannot change outside the current control flow.

`volatile` does not provide mutual exclusion, atomic read-modify-write operations, or general synchronization guarantees. The safety of this example also relies on the single-producer/single-consumer ownership model and appropriately sized/aligned index accesses on the target.

## Cortex-M3 Concepts

### NVIC and Interrupt Handling

1. **Peripheral interrupt enable:** `UART0->CTRL |= CM3DS_MPS2_UART_CTRL_RXIRQEN_Msk`
2. **NVIC interrupt enable:** `NVIC_EnableIRQ(UART0_IRQn)`
3. UART0 receives a byte and asserts its RX interrupt
4. The Cortex-M3 vectors to `UART0_Handler()` when the interrupt is enabled and not otherwise masked
5. The ISR reads the received byte and acknowledges the UART RX interrupt through `INTCLEAR`

### Single-Producer / Single-Consumer Ownership

This ring buffer uses a single-producer/single-consumer design. No lock is used in this lab because each index has exactly one writer. The design also relies on the target performing the index accesses as indivisible accesses.

This should not be generalized to multiple producers or consumers. `volatile` does not turn compound read-modify-write expressions into atomic operations and does not provide general synchronization.

## Debugging Session

### Set Breakpoints
```gdb
(gdb) break UART0_Handler
(gdb) break main.c:23
(gdb) continue
```

### Examine Ring Buffer State
```gdb
(gdb) print rx_rb
$1 = {
  data = "Hello\000\000...",
  head = 5,
  tail = 0
}

(gdb) print rx_rb.head
$2 = 5

(gdb) print rx_rb.tail
$3 = 0

# Calculate number of items
(gdb) print (rx_rb.head - rx_rb.tail + 64) % 64
$4 = 5
```

### Watch for Buffer Full/Empty
```gdb
(gdb) watch rx_rb.head
(gdb) watch rx_rb.tail

# Continue and observe buffer changes
(gdb) continue
```

### Inspect ISR Execution
```gdb
(gdb) break UART0_Handler
(gdb) continue
(gdb) next
(gdb) print /c c          # Print as character
(gdb) print rx_rb.head
```
### Examine ring-buffer storage as characters
```gdb
# View buffer data
(gdb) x/64c &rx_rb.data
0x20000000:     72 'H'  101 'e' 108 'l' 108 'l' 111 'o'

# View buffer as hex
(gdb) x/64xb &rx_rb.data

# View current head/tail addresses
(gdb) print &rx_rb.data[rx_rb.head]
(gdb) print &rx_rb.data[rx_rb.tail]
```

## References

- ARM Cortex-M3 Technical Reference Manual
- ARMv7-M Architecture Reference Manual (Chapter B1: Exception Model)
- CMSIS Core Documentation
- Producer-Consumer Pattern in Embedded Systems

## Key Takeaways

✓ Ring buffers decouple data reception from processing and can absorb temporary bursts, up to their available capacity 
✓ ISRs should be fast - just capture data and return  
✓ Producer-consumer pattern decouples ISR from processing  
✓ Volatile preserves required shared-state accesses in this bare-metal model but does not provide locking or general synchronization 
✓ Aligned index accesses and single-writer ownership avoid torn/shared read-modify-write updates in this design
✓ A finite ring buffer absorbs only temporary bursts; data can still be lost when the buffer becomes full
✓ Buffer sizing must account for receive rate and the maximum time the consumer may be delayed
