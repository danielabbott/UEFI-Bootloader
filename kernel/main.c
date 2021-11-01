#include <stdint.h>

int j;

void io_out8(uint16_t port, uint8_t data);
void io_out16(uint16_t port, uint8_t data);
void io_out32(uint16_t port, uint8_t data);

uint8_t io_in8(uint16_t port);
uint16_t io_in16(uint16_t port);
uint32_t io_in32(uint16_t port);

void cli_hlt(void);

void load_gdt(void);
void load_idt(void);

typedef struct AMD64IDT {
    uint16_t offset_1; // offset bits 0..15
    uint16_t selector; // 1
    uint8_t ist;       // bits 0..2 holds Interrupt Stack Table offset, rest of bits zero.
    uint8_t type_attr; // type and attributes
    uint16_t offset_2; // offset bits 16..31
    uint32_t offset_3; // offset bits 32..63
    uint32_t zero;     // reserved
} AMD64IDT;

AMD64IDT amd64_idt[256];
void default_interrupt_handler(); // DO NOT CALL THIS!

void amd64_cli(void);
void amd64_sti(void);

void kernel_main(void * arg0, void * arg1, void * fb, unsigned int fb_size);
void kernel_main(void * arg0, void * arg1, void * fb, unsigned int fb_size)
{
    (void) arg0;
    (void) arg1;

    if(fb && fb_size) {
        for(unsigned int i = 0; i < fb_size; i++) {
            ((uint8_t *)fb)[i] = 0;
        }
    }


    // https://wiki.osdev.org/Serial_Ports
    const uint16_t PORT = 0x3f8;

    io_out8(PORT + 1, 0x00);    // Disable all interrupts
    io_out8(PORT + 3, 0x80);    // Enable DLAB (set baud rate divisor)
    io_out8(PORT + 0, 0x03);    // Set divisor to 3 (lo byte) 38400 baud
    io_out8(PORT + 1, 0x00);    //                  (hi byte)
    io_out8(PORT + 3, 0x03);    // 8 bits, no parity, one stop bit
    io_out8(PORT + 2, 0xC7);    // Enable FIFO, clear them, with 14-byte threshold
    io_out8(PORT + 4, 0x0B);    // IRQs enabled, RTS/DSR set
    io_out8(PORT + 4, 0x1E);    // Set in loopback mode, test the serial chip
    io_out8(PORT + 0, 0xAE);    // Test serial chip (send byte 0xAE and check if serial returns same byte)

    // Check if serial is faulty (i.e: not same byte as sent)
    if(io_in8(PORT + 0) != 0xAE) {
        cli_hlt();
    }

    // If serial is not faulty set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    io_out8(PORT + 4, 0x0F);

    #define WRITE(c) while ((io_in8(PORT + 5) & 0x20) == 0); io_out8(PORT, c);

    WRITE('k');
    WRITE('e');
    WRITE('r');
    WRITE('n');
    WRITE('e');
    WRITE('l');

    amd64_cli();
    load_gdt();

    for(unsigned int i = 0; i < 256; i++) {
        amd64_idt[i].offset_1 = (uint16_t)(uintptr_t)default_interrupt_handler;
        amd64_idt[i].selector = 1;
        amd64_idt[i].type_attr = 0x8e;
        amd64_idt[i].offset_2 = (uint16_t)((uintptr_t)default_interrupt_handler >> 16);
        amd64_idt[i].offset_3 = (uint32_t)((uintptr_t)default_interrupt_handler >> 32);
    }

    load_idt();
    amd64_sti();

    WRITE(' ');
    WRITE('l');
    WRITE('o');
    WRITE('a');
    WRITE('d');
    WRITE('e');
    WRITE('d');
    WRITE('\n');

    cli_hlt();
}

