ENTRY(kernel_main)
SECTIONS
{
    KERNEL_VMA = -2097152;
    . = KERNEL_VMA;

    .text : ALIGN(4096) 
    {
        _code = .;
        *(.text)
        *(.rodata*)
    }

   .data : ALIGN(4096)
   {
        _data = .;
        *(.data)
   }

   .bss : ALIGN(4096)
   {
       _bss = .;
       *(.bss)
   }

   _end = .;

   /DISCARD/ :
   {
        *(.comment)
   }
}