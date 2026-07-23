# Lab 11: UART Ring Buffer

## Overview

This lab demonstrates interrupt-driven UART communication with a ring buffer (circular buffer) for asynchronous data handling. It builds upon Lab 10 by introducing a producer-consumer pattern where the ISR captures incoming data and the main loop processes it independently.

## Learning Objectives

- Implement a circular ring buffer data structure
- Understand producer-consumer patterns in embedded systems
- Learn proper use of `volatile` keyword for shared data
- Practice interrupt-driven I/O with buffering
- Understand the importance of keeping ISRs short and fast
- Explore atomic operations on Cortex-M3
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
```

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
- Can take time without blocking reception

### Volatile Keyword

All ring buffer fields are marked `volatile`:

```c
typedef struct {
    volatile char data[RB_SIZE];
    volatile uint32_t head;
    volatile uint32_t tail;
} ringbuffer_t;
```

**Why volatile is essential:**
- The ISR (interrupt context) modifies `head` and reads `tail`
- The main loop (thread context) modifies `tail` and reads `head`
- Without `volatile`, the compiler might cache values in registers
- This could lead to stale data and race conditions

## Cortex-M3 Concepts

### NVIC and Interrupt Handling

1. **Peripheral interrupt enable:** `UART_CTRL_RXIRQEN_Msk`
2. **NVIC enable:** Done automatically via CMSIS
3. **Interrupt trigger:** UART RX buffer has data
4. **Interrupt clear:** `UART0->INTCLEAR` prevents re-entry

### Atomic Operations

On Cortex-M3, 32-bit aligned loads/stores are atomic:
- `head` and `tail` are 32-bit aligned
- Single producer (ISR writes head)
- Single consumer (main writes tail)
- No explicit locking needed for this pattern

**Warning:** This would NOT be safe with:
- Multiple producers or consumers
- Non-atomic operations (e.g., read-modify-write)
- Multi-core systems

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

# When breakpoint hits
(gdb) info registers
(gdb) print c
(gdb) print /c c          # Print as character
(gdb) print rx_rb.head
```

### Check UART Registers
```gdb
(gdb) print /x *(UART_TypeDef*)0x40004000
$5 = {
  DATA = 0x41,        # 'A'
  STATE = 0x1,        # RX buffer full
  CTRL = 0x7,         # TX, RX, RX IRQ enabled
  INTCLEAR = 0x0,
  BAUDDIV = 0xd9
}
```

### Examine Buffer Contents
```gdb
# View buffer data as string
(gdb) x/64c &rx_rb.data
0x20000000:     72 'H'  101 'e' 108 'l' 108 'l' 111 'o'

# View buffer as hex
(gdb) x/64xb &rx_rb.data

# View head/tail pointers
(gdb) print &rx_rb.data[rx_rb.head]
(gdb) print &rx_rb.data[rx_rb.tail]
```

## References

- ARM Cortex-M3 Technical Reference Manual
- ARMv7-M Architecture Reference Manual (Chapter B1: Exception Model)
- CMSIS Core Documentation
- Producer-Consumer Pattern in Embedded Systems

## Key Takeaways

✓ Ring buffers enable asynchronous I/O without data loss  
✓ ISRs should be fast - just capture data and return  
✓ Producer-consumer pattern decouples ISR from processing  
✓ Volatile is essential for shared data between ISR and main  
✓ Cortex-M3 provides atomic 32-bit operations for simple synchronization  
✓ Buffer size must account for worst-case latency  
✓ This pattern scales to multiple buffered peripherals
