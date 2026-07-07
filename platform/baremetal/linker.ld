ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) :  ORIGIN = 0x00000000, LENGTH = 256K
    RAM   (rwx):  ORIGIN = 0x20000000, LENGTH = 64K
}

STACK_SIZE = 0x1000; /* 4 KB */
HEAP_SIZE  = 0x0800; /* 2 KB */

_sidata = LOADADDR(.data);

SECTIONS
{
    .text :
    {
	    KEEP(*(.isr_vector))
	    *(.text*)
	    *(.rodata*)
    } > FLASH

    .data :
    {
	    _sdata = .;
	    *(.data*)
	    _edata = .;
    } > RAM AT > FLASH

    .bss :
    {
	    _sbss = .;
	    *(.bss*)
	    *(COMMON)
	    _ebss = .;
    } > RAM

    /*----------------------*/
    /* Heap                 */
    /*----------------------*/
    .heap (NOLOAD):
    {
	    . = ALIGN(8);
	    __heap_start__ = .;
	    . += HEAP_SIZE;
	    __heap_end__ = .;
    } > RAM

     /*----------------------*/  
     /* Stack                */
     /*----------------------*/
     .stack (NOLOAD):
     {
	     . = ALIGN(8);
	     __stack_limit = .;
	     . += STACK_SIZE;
	     __stack_top = .;
     } > RAM

     _end = .;
}
