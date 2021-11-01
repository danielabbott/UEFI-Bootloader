BITS 64
SECTION .text


; void set_cr3(uintptr_t);
global set_cr3
set_cr3:
    mov cr3, rdi
    ret