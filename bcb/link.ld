MEMORY{
    FLASH(rw) : ORIGIN = 0x08008000, LENGTH = 16K
    RAM(rwx)    : ORIGIN = 0x20000000, LENGTH = 96K
}

SECTIONS
{
    /* Puts the BCB at the very start of FLASH */
    .bcb ORIGIN(FLASH) :
    {
        KEEP(*(.bcb))
    } > FLASH


    /DISCARD/ :
    {
        *(.text*)
        *(.rodata*)
        *(.data*)
        *(.bss*)
        *(.comment*)
        *(.ARM.exidx*)
        *(.ARM.attributes*)
        *(*)
    }
}
