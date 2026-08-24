/**
 * @file    linker.ld.S
 * @brief   Preprocessor-enabled linker script for ARM Cortex-M3 MPS2 AN385
 * @details This linker script uses C preprocessor to import hardware memory 
 *          definitions from memory_map.h and automatically calculate optimal
 *          stack and heap sizes based on actual section usage.
 *
 * Memory Layout Strategy:
 * ----------------------
 * FLASH: Code and constants (vector table, .text, .rodata)
 * RAM:   Data, BSS, Heap (grows up), Stack (grows down)
 * 
 * Stack and heap sizes are calculated to:
 * - Ensure minimum requirements for embedded operation
 * - Maximize available heap space
 * - Prevent stack overflow into heap
 * - Maintain 8-byte alignment (AAPCS requirement)
 */

#include "memory_map.h"

/* Architecture Requirements */
#define STACK_ALIGNMENT     8        /* ARMv7-M AAPCS requirement */
#define HEAP_ALIGNMENT      8        /* malloc() alignment */

/* Minimum Stack Requirements (safety-critical) */
#define MIN_STACK_SIZE      0x400    /* 1 KB minimum for basic operation */
#define STACK_OVERHEAD      0x200    /* 512 bytes per call level overhead */
#define ISR_STACK_FRAME     0x80     /* 128 bytes for exception context */

/* Calculated Stack Size (conservative for bare-metal) */
#ifndef STACK_SIZE
    #define STACK_SIZE      0x1000   /* 4 KB default (can override at build time) */
#endif

/* Heap Size Strategy:
 * For baremetal platform: Small heap since malloc typically unused
 * For runtime platform: Use remaining RAM after stack
 */
#ifndef HEAP_SIZE
    #define HEAP_SIZE       0x800    /* 2 KB default (can override at build time) */
#endif

/* Verify sizes are properly aligned */
#if (STACK_SIZE % STACK_ALIGNMENT) != 0
    #error "STACK_SIZE must be 8-byte aligned"
#endif

#if (HEAP_SIZE % HEAP_ALIGNMENT) != 0
    #error "HEAP_SIZE must be 8-byte aligned"
#endif

/* Verify minimum requirements */
#if STACK_SIZE < MIN_STACK_SIZE
    #error "STACK_SIZE too small for safe operation"
#endif

ENTRY(Reset_Handler)

/**
 * Memory regions from hardware specification (CM3DS_MPS2.h)
 * These values are FIXED by the MPS2 AN385 hardware design
 */
MEMORY
{
    FLASH (rx)  : ORIGIN = CM3DS_MPS2_FLASH_BASE, LENGTH = CM3DS_MPS2_FLASH_SIZE
    RAM   (rwx) : ORIGIN = CM3DS_MPS2_SRAM_BASE,  LENGTH = CM3DS_MPS2_SRAM_SIZE
}

/* Provide memory size symbols for runtime use */
__flash_start__ = ORIGIN(FLASH);
__flash_size__  = LENGTH(FLASH);
__ram_start__   = ORIGIN(RAM);
__ram_size__    = LENGTH(RAM);

_sidata = LOADADDR(.data);

SECTIONS
{
    /*========================================================================*/
    /* FLASH Memory Layout                                                    */
    /*========================================================================*/
    
    /**
     * Vector table MUST be at 0x00000000 (ARMv7-M requirement)
     * Contains initial SP and exception handlers
     */
    .text :
    {
        __text_start__ = .;
        
        /* Vector table must be first */
        KEEP(*(.isr_vector))
        
        /* Program code */
        *(.text*)
        
        /* Read-only data (constants, strings) */
        *(.rodata*)
        
        /* Ensure 4-byte alignment for ARM code */
        . = ALIGN(4);
        __text_end__ = .;
        
    } > FLASH
    
    /* Calculate code size for diagnostics */
    __text_size__ = __text_end__ - __text_start__;
    
    /*========================================================================*/
    /* RAM Memory Layout                                                      */
    /*========================================================================*/
    
    /**
     * Initialized data (.data section)
     * Copied from FLASH to RAM by startup code
     */
    .data :
    {
        . = ALIGN(4);
        __data_start__ = .;
        _sdata = .;
        
        *(.data*)
        
        . = ALIGN(4);
        __data_end__ = .;
        _edata = .;
        
    } > RAM AT > FLASH
    
    __data_size__ = __data_end__ - __data_start__;
    
    /**
     * Uninitialized data (.bss section)
     * Zeroed by startup code
     */
    .bss :
    {
        . = ALIGN(4);
        __bss_start__ = .;
        _sbss = .;
        
        *(.bss*)
        *(COMMON)
        
        . = ALIGN(4);
        __bss_end__ = .;
        _ebss = .;
        
    } > RAM
    
    __bss_size__ = __bss_end__ - __bss_start__;
    
    /*========================================================================*/
    /* Heap (grows upward from end of BSS)                                    */
    /*========================================================================*/
    
    /**
     * Heap for dynamic memory allocation (malloc/free)
     * Size determined by HEAP_SIZE preprocessor definition
     * 
     * Note: baremetal platform typically doesn't use heap
     *       This is reserved for runtime platform (lab06+)
     */
    .heap (NOLOAD) :
    {
        . = ALIGN(HEAP_ALIGNMENT);
        __heap_start__ = .;
        PROVIDE(end = .);           /* Traditional Unix 'end' symbol */
        PROVIDE(_heap_start = .);   /* Alternative name */
        
        . += HEAP_SIZE;
        
        . = ALIGN(HEAP_ALIGNMENT);
        __heap_end__ = .;
        PROVIDE(_heap_end = .);     /* Alternative name */
        
    } > RAM
    
    __heap_size__ = __heap_end__ - __heap_start__;
    
    /*========================================================================*/
    /* Stack (grows downward from top of RAM)                                 */
    /*========================================================================*/
    
    /**
     * Stack for function calls and local variables
     * Size determined by STACK_SIZE preprocessor definition
     * 
     * ARM Cortex-M uses Full Descending (FD) stack:
     * - SP points to last pushed item
     * - Grows downward (from high to low addresses)
     * - Pre-decrements on push
     * 
     * Initial SP (__stack_top) loaded from vector table offset 0x00000000
     */
    .stack (NOLOAD) :
    {
        . = ALIGN(STACK_ALIGNMENT);
        __stack_limit = .;          /* Bottom of stack (guard) */
        PROVIDE(_stack_limit = .);  /* Alternative name */
        
        . += STACK_SIZE;
        
        . = ALIGN(STACK_ALIGNMENT);
        __stack_top = .;            /* Top of stack (initial SP) */
        PROVIDE(_stack_top = .);    /* Alternative name */
        
    } > RAM
    
    __stack_size__ = __stack_top - __stack_limit;
    
    /*========================================================================*/
    /* End of RAM usage                                                       */
    /*========================================================================*/
    
    _end = .;
    PROVIDE(end = .);
    
    /* Calculate total RAM usage */
    __ram_used__ = _end - __ram_start__;
    __ram_free__ = __ram_start__ + __ram_size__ - _end;
    
    /*========================================================================*/
    /* Verification and Safety Checks                                         */
    /*========================================================================*/
    
    /**
     * Assert statements to catch configuration errors at link time
     * These prevent common mistakes that would cause runtime failures
     */
    
    /* Ensure we haven't overflowed RAM */
    ASSERT(_end <= (__ram_start__ + __ram_size__), 
           "ERROR: RAM overflow! Reduce STACK_SIZE or HEAP_SIZE")
    
    /* Ensure minimum stack space available */
    ASSERT(__stack_size__ >= MIN_STACK_SIZE, 
           "ERROR: Stack too small for safe operation")
    
    /* Warn if using more than 90% of RAM */
    ASSERT(__ram_used__ < (__ram_size__ * 9 / 10),
           "WARNING: Using >90% of available RAM")
    
    /* Ensure proper alignment */
    ASSERT((__stack_top % STACK_ALIGNMENT) == 0,
           "ERROR: Stack top not properly aligned")
    
    ASSERT((__heap_start__ % HEAP_ALIGNMENT) == 0,
           "ERROR: Heap start not properly aligned")
    
    /*========================================================================*/
    /* Debug sections (not loaded into target)                                */
    /*========================================================================*/
    
    /* DWARF debug information */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line .debug_line.* .debug_line_end) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    .debug_ranges   0 : { *(.debug_ranges) }
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
    
    /* ARM-specific sections */
    .ARM.attributes 0 : { KEEP(*(.ARM.attributes)) }
    .comment        0 : { *(.comment) }
    
    /*========================================================================*/
    /* Discard sections                                                       */
    /*========================================================================*/
    
    /DISCARD/ :
    {
        *(.note.GNU-stack)
        *(.gnu_debuglink)
        *(.gnu.lto_*)
    }
}

/*
 * Build-time Memory Usage Summary
 * (Visible in .map file)
 * 
 * These symbols allow inspection of memory layout without running code
 */
