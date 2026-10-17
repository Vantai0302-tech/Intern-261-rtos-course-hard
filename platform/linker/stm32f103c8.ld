ENTRY(Reset_Handler)
MEMORY
{
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 20K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Stack_Size = 1024;
SECTIONS
{
    .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) } > FLASH
    .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
    .ARM.extab : { *(.ARM.extab*) } > FLASH
    .ARM.exidx : { *(.ARM.exidx*) } > FLASH
    .data :
    {
        . = ALIGN(4); _sdata = .;
        *(.data*)
        . = ALIGN(4); _edata = .;
    } > RAM AT > FLASH
    _sidata = LOADADDR(.data);
    .bss (NOLOAD) :
    {
        . = ALIGN(8); _sbss = .;
        *(.bss*) *(COMMON)
        . = ALIGN(8); _ebss = .;
    } > RAM
    .noinit (NOLOAD) :
    {
        . = ALIGN(8); _snoinit = .;
        *(.noinit*)
        . = ALIGN(8); _enoinit = .;
    } > RAM
    .stack _estack - _Min_Stack_Size (NOLOAD) :
    {
        _sstack = .; . += _Min_Stack_Size;
    } > RAM
    ASSERT(_enoinit <= _sstack, "RAM overlaps main/ISR stack")
    /DISCARD/ : { *(.comment) *(.note*) }
}
