/* Minimal AArch64 linker script for QEMU virt */
ENTRY(_start)

MEMORY
{
	RAM (rwx) : ORIGIN = 0x40200000, LENGTH = 0x0FE00000
}

SECTIONS
{
    .text : ALIGN(16)
    {
	    KEEP(*(.text.boot))
	    *(.text .text.*)
    } > RAM

    /* EL0 executable region: complete 4 KiB MMU pages */
    .el0_text : ALIGN(4096)
    {
	    el0_code_start = .;

	    KEEP(*(.el0_text .el0_text.*))
	    . = ALIGN(4096);
	    el0_code_end = .;
    } > RAM

    .rodata : ALIGN(16)
    {
	    *(.rodata .rodata.*)
    } > RAM

    .data : ALIGN(16)
    {
	    *(.data .data.*)
    } > RAM

    .bss (NOLOAD) : ALIGN(16)
    {
	    *(.bss .bss.*)
	    *(COMMON)
    } > RAM

    .stack (NOLOAD) : ALIGN(16)
    {
	    . = . + 0x4000;
	    . = ALIGN(16);
	    __stack_top = .;
    } > RAM

    /DISCARD/ :
    {
	    *(.comment)
	    *(.note*)
	    *(.eh_frame*)
    }
}
