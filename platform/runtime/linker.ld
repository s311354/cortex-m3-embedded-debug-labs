ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) :  ORIGIN = 0x00000000, LENGTH = 256K
    RAM   (rwx):  ORIGIN = 0x20000000, LENGTH = 64K
}

_sidata = LOADADDR(.data);

SECTIONS
{
    /*---------------------------------*/
    /* Vector Table                    */
    /*---------------------------------*/
    .text :
    {
	    KEEP(*(.isr_vector))
	    *(.text*)
	    *(.rodata*)

    } > FLASH

    /*---------------------------------*/
    /* C Runtime Tables                */  
    /*---------------------------------*/
    .preinit_array :
    {
	    . = ALIGN(4);
	    
	    __preinit_array_start = .;
	    KEEP(*(.preinit_array))
	    KEEP(*(.preinit_array.*))
	    __preinit_array_end = .;

    } > FLASH

    .init_array :
    {
	    . = ALIGN(4);
	    
	    __init_array_start = .;
	    KEEP(*(SORT(.init_array.*)))
	    KEEP(*(.init_array))
	    __init_array_end = .;

    } > FLASH

    .fini_array :
    {
	    . = ALIGN(4);
	    
	    __fini_array_start = .;
	    KEEP(*(SORT(.fini_array.*)))
	    KEEP(*(.fini_array))
	    __fini_array_end = .;

    } > FLASH

    /*---------------------------------*/
    /* Initialized Data                */  
    /*---------------------------------*/
    .data :
    {
	    . = ALIGN(4);

	    _sdata = .;
	    *(.data*)

	    . = ALIGN(4);
	    _edata = .;

    } > RAM AT > FLASH

    .bss :
    {
	    . = ALIGN(4);

	    _sbss = .;
	    *(.bss*)
	    *(COMMON)

	    . = ALIGN(4);
	    _ebss = .;

    } > RAM


    _end = .;
}
