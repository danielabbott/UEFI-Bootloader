#pragma once

#include <stdint.h>

typedef struct ElfHeader
{
	uint32_t magic; // 0x464c457f
	uint8_t class; // 2
	uint8_t lsb_msb; // 1
	uint8_t version; // 1
	uint8_t padding[9];
	uint16_t type; // 2
	uint16_t machine; // 62
	uint32_t version2; // 1
	uintptr_t entry; // virtual address of kernel_main
	uintptr_t program_header_offset; // for running an executable
	uintptr_t section_header_offset; // for linking
	uint32_t flags; // unused
	uint16_t header_size; // 64
	uint16_t program_header_entry_size; // >= 56
	uint16_t program_header_count; // < 0xffff

	// For linking
	uint16_t section_header_entry_size;
	uint16_t section_header_count;
	uint16_t string_section_index;
} ElfHeader;

typedef struct ElfProgramHeader
{
	uint32_t type; // 1 if this should be loaded
	uint32_t flags; // read = 0x4, write = 0x2, execute = 0x1
	uintptr_t file_offset;
	uintptr_t virtual_address;
	uintptr_t physical_address; // ignore
	uintptr_t size_on_disk;
	uintptr_t size_in_memory;
	uintptr_t align; // 4096
} ElfProgramHeader;