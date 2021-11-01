BITS 64
SECTION .text


; void io_out8(uint16_t port, uint8_t data);
global io_out8
io_out8:
    mov dx, di
    mov al, sil
    out dx, al
    ret

; void io_out16(uint16_t port, uint16_t data);
global io_out16
io_out16:
    mov dx, di
    mov ax, si
    out dx, ax
    ret

; void io_out32(uint16_t port, uint32_t data);
global io_out32
io_out32:
    mov dx, di
    mov eax, esi
    out dx, eax
    ret

; uint8_t io_in8(uint16_t port);
global io_in8
io_in8:
    mov dx, di
    xor rax, rax
    in al, dx
    ret


; uint16_t io_in16(uint16_t port);
global io_in16
io_in16:
    mov dx, di
    xor rax, rax
    in ax, dx
    ret

; uint32_t io_in32(uint16_t port);
global io_in32
io_in32:
    mov dx, di
    xor rax, rax
    in eax, dx
    ret

