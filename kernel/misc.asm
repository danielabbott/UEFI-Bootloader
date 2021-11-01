BITS 64
SECTION .text


; void cli_hlt(void);
global cli_hlt
cli_hlt:
    cli
    hlt
    ret ; should not get to here


; void amd64_cli(void);
global amd64_cli
amd64_cli:
    cli
    ret

; void amd64_sti(void);
global amd64_sti
amd64_sti:
    sti
    ret

; void load_gdt(void);
global load_gdt
load_gdt:
    lgdt [LGDT_DATA]

SECTION .rodata

; https://wiki.osdev.org/Setting_Up_Long_Mode

GDT64:
; NULL
dq 0

; CODE
dw 0                         ; Limit (low).
dw 0                         ; Base (low).
db 0                         ; Base (middle)
db 10011010b                 ; Access (exec/read).
db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
db 0                         ; Base (high).

; DATA
dw 0                         ; Limit (low).
dw 0                         ; Base (low).
db 0                         ; Base (middle)
db 10010010b                 ; Access (read/write).
db 00000000b                 ; Granularity.
db 0                         ; Base (high).

LGDT_DATA:
dw $ - GDT64 - 1             ; Limit.
dq GDT64                     ; Base.

SECTION .text

; void load_idt();
global load_idt
load_idt:
    LIDT [LIDT_DATA]
    ret

global default_interrupt_handler
default_interrupt_handler:
    iretq

SECTION .data
LIDT_DATA:
    dw 4095
    extern amd64_idt
    dq amd64_idt