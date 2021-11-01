#include <efi/efi.h>
#include <efi/efilib.h>
#include <stdint.h>
#include <stdbool.h>
#include "ELF.h"

typedef void (*KernelMainFunction)(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);
KernelMainFunction kernel_main;

const uint64_t kernel_base_address = 0xFFFFFFFFFFE00000;

static void sleep_3_seconds(void)
{
	uefi_call_wrapper((void*)BS->Stall, 1, 3000000ull);
}

static void err(EFI_STATUS e)
{
	Print(L"Fatal error: %u\r\n", (uint32_t)e);
	sleep_3_seconds();
}

static EFI_STATUS get_device_handle(EFI_HANDLE ImageHandle, EFI_HANDLE * device_handle)
{
	#define EFI_ASSERT(x) { \
		EFI_STATUS status = x; \
		if (EFI_ERROR(status)) {return status;} \
	}

	EFI_LOADED_IMAGE_PROTOCOL* loaded_image_protocol;

	EFI_ASSERT(uefi_call_wrapper((void*)BS->OpenProtocol, 6, ImageHandle, &LoadedImageProtocol, &loaded_image_protocol,
		ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL));

	Print(L"LoadedImage protocol loaded\r\n");
	*device_handle = loaded_image_protocol->DeviceHandle;

	return EFI_SUCCESS;
}

static EFI_STATUS load_kernel_file(EFI_HANDLE ImageHandle, EFI_HANDLE device_handle, void ** buffer, uint32_t * kernel_file_size)
{
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* file_system_protocol;
	EFI_ASSERT(uefi_call_wrapper((void*)BS->OpenProtocol, 6, device_handle, &FileSystemProtocol, &file_system_protocol,
		ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL));

	Print(L"FileSystem protocol loaded\r\n");

	EFI_FILE_PROTOCOL* root;
	EFI_ASSERT(uefi_call_wrapper((void*)file_system_protocol->OpenVolume, 2, file_system_protocol, &root));
	Print(L"Volume opened\r\n");


	EFI_FILE_PROTOCOL* kernel_file;
	EFI_ASSERT(uefi_call_wrapper((void*)root->Open, 5, root, &kernel_file, L"kernel", EFI_FILE_MODE_READ, 0));
	Print(L"Kernel file opened\r\n");


	EFI_FILE_INFO *info = LibFileInfo(kernel_file);
	if(!info) return 1;

	*kernel_file_size = (uint32_t)info->FileSize;
	FreePool(info);

	Print(L"Kernel file size: %u\r\n", *kernel_file_size);


	unsigned int pages = ((uint32_t)*kernel_file_size+4095)/4096;
	*buffer = NULL;
	EFI_ASSERT(uefi_call_wrapper((void*)BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages, buffer));
	Print(L"Memory allocated\r\n");


	UINTN buffer_size = *kernel_file_size;
	EFI_ASSERT(uefi_call_wrapper((void*)kernel_file->Read, 3, kernel_file, &buffer_size, *buffer));

	if(buffer_size < *kernel_file_size) {
		return 1;
	}

	Print(L"Kernel file read into memory\r\n");


	EFI_ASSERT(uefi_call_wrapper((void*)kernel_file->Close, 1, kernel_file));
	Print(L"Kernel file closed\r\n");


	EFI_ASSERT(uefi_call_wrapper((void*)root->Close, 1, root));
	Print(L"Volume closed\r\n");

	return EFI_SUCCESS;
}

static int validate_kernel_elf(uint32_t * pages_needed, void * kernel_elf, uint32_t kernel_file_size)
{
	*pages_needed = 0;

	if(kernel_file_size < sizeof(ElfHeader) + sizeof(ElfProgramHeader)*2) return 1;

	ElfHeader * header = kernel_elf;

	if(kernel_file_size < header->header_size) return 1;

	if(kernel_file_size < 
		header->program_header_offset + header->program_header_entry_size * header->program_header_count) return 1;

	// sanity checks on header values

	if(
		header->magic != 0x464c457f ||
		header->class != 2 ||
		header->lsb_msb != 1 ||
		header->version != 1  ||
		header->type != 2 ||
		header->machine != 62 ||
		header->version2 != 1  ||
		!header->entry ||
		header->entry < kernel_base_address ||
		!header->program_header_offset ||
		header->header_size < 64 ||
		header->program_header_entry_size < 56 ||
		header->program_header_count < 2 ||
		header->program_header_count == 0xffff
	) return 1;

	kernel_main = (KernelMainFunction) header->entry;

	ElfProgramHeader * program_header = (void *)((uintptr_t)kernel_elf + header->program_header_offset);
	for(unsigned int i = 0; i < header->program_header_count; i++) {
		// read = 0x4, write = 0x2, execute = 0x1
		if(program_header->type == 1 && (program_header->flags & 4)) {

			if(	!program_header->virtual_address ||
				program_header->virtual_address % 4096 != 0 ||
				program_header->file_offset % 4096 != 0 ||
				program_header->size_on_disk > program_header->size_in_memory ||
				program_header->file_offset + program_header->size_on_disk > (uint64_t)kernel_file_size ||
				!program_header->size_in_memory ||
				program_header->align != 4096
			) return 1;

			unsigned int mem = ((unsigned int)program_header->size_in_memory + 4095) / 4096;
			unsigned int disk = ((unsigned int)program_header->size_on_disk + 4095) / 4096;

			*pages_needed += mem - disk;
		}

		program_header = (void *)((uintptr_t)program_header + header->program_header_entry_size);
	}


	return 0;
}

// n = number of 64-bit zeros to write
static void memclr64(uint64_t * dst, unsigned int n)
{
	for(unsigned int i = 0; i < n; i++) {
		dst[i] = 0;
	}
}


void set_cr3(uintptr_t);

// PML4: Represents 256TiB 
static uintptr_t pt_l4[512] __attribute__((aligned(4096))); 

// PDPT: Each of these represents 512GiB
static uintptr_t pt_l3_0[512] __attribute__((aligned(4096))); 
static uintptr_t pt_l3_last[512] __attribute__((aligned(4096))); 

// PDT: Each of these represents 1GiB.
// pt_l2 is 512 of these, so 512GiB
static uintptr_t pt_l2[512*512] __attribute__((aligned(4096))); 
static uintptr_t pt_l2_last[512] __attribute__((aligned(4096))); 

// PT: Highest page table- 2MiB
static uintptr_t pt_l1_last[512] __attribute__((aligned(4096))); 

// Adds 4KiB pages to pt_l1_last
static void add_kernel_pages(void * kernel_elf, void * kernel_0_pages)
{
	ElfHeader * header = kernel_elf;

	uintptr_t kernel_0_pages_ptr = (uintptr_t)kernel_0_pages;

	ElfProgramHeader * program_header = (void *)((uintptr_t)kernel_elf + header->program_header_offset);
	for(unsigned int i = 0; i < header->program_header_count; i++) {
		// read = 0x4, write = 0x2, execute = 0x1
		if(program_header->type == 1 && (program_header->flags & 4)) {

			unsigned int first_page_index = (unsigned int) (program_header->virtual_address - 
				kernel_base_address) / 4096;
			
			unsigned int pages = ((unsigned int) program_header->size_in_memory + 4095) / 4096;
			unsigned int disk_pages = ((unsigned int)program_header->size_on_disk + 4095) / 4096;
			unsigned int zero_pages = pages - disk_pages;

			unsigned int page_idx = first_page_index;
			unsigned int section_page_idx = 0;
			uintptr_t elf_data_ptr = (uintptr_t)kernel_elf;

			// Code/data pages (map loaded ELF file)
			for(unsigned int j = 0; j < disk_pages; j++, page_idx++, section_page_idx++) {
				uintptr_t x = elf_data_ptr + program_header->file_offset + section_page_idx*4096;
				pt_l1_last[page_idx] = x | 3;
			}

			// Zeroed pages
			for(unsigned int j = 0; j < zero_pages; j++, page_idx++, section_page_idx++, kernel_0_pages_ptr += 4096) {
				pt_l1_last[page_idx] = kernel_0_pages_ptr | 3;
			}
		}

		program_header = (void *)((uintptr_t)program_header + header->program_header_entry_size);
	}
}

static void setup_paging(void * kernel_elf, void * kernel_0_pages)
{
	// identity map first 512GiB

	pt_l4[0] = (uintptr_t)&pt_l3_0[0] | 3; // 3 = Present & Writeable
	
	for(unsigned int i = 0; i < 512; i++) {
		pt_l3_0[i] = (uintptr_t)&pt_l2[i*512] | 3;
	}

	for(unsigned int i = 0; i < 512*512; i++) {
		pt_l2[i] = i * 2*1024*1024;
		pt_l2[i] |= 3 | (1 << 7); // bit 7 = 2MiB pages	
	}


	// Kernel pages



	pt_l4[511] = (uintptr_t)&pt_l3_last[0] | 3;
	pt_l3_last[511] = (uintptr_t)&pt_l2_last[0] | 3;
	pt_l2_last[511] = (uintptr_t)&pt_l1_last[0] | 3;

	add_kernel_pages(kernel_elf, kernel_0_pages);


	uintptr_t cr3 = ((uintptr_t)&pt_l4[0]);
	set_cr3(cr3);
}

// static EFI_STATUS get_framebuffer(EFI_HANDLE ImageHandle, EFI_HANDLE device_handle,
// 	void ** fb, unsigned int * framebuffer_size)
// {
// 	*fb = NULL;

// 	EFI_GRAPHICS_OUTPUT_PROTOCOL* graphics_output_protocol;
// 	EFI_ASSERT(uefi_call_wrapper((void*)BS->OpenProtocol, 6, ImageHandle, 
// 		&GraphicsOutputProtocol, &graphics_output_protocol,
// 		ImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL));

// 	Print(L"Graphics output protocol loaded\r\n");

// 	EFI_GRAPHICS_PIXEL_FORMAT pixel_format = graphics_output_protocol->Mode->Info->PixelFormat;
// 	if(pixel_format == PixelBltOnly) {
// 		return 1;
// 	}

// 	*fb = (void *)graphics_output_protocol->Mode->FrameBufferBase;
// 	*framebuffer_size = (unsigned)graphics_output_protocol->Mode->FrameBufferSize;

// 	return EFI_SUCCESS;
// }

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable);

EFI_STATUS
EFIAPI
efi_main (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable)
{
	InitializeLib(ImageHandle, SystemTable);

	#undef EFI_ASSERT
	#define EFI_ASSERT(x) { \
			EFI_STATUS status = x; \
			if (EFI_ERROR(status)) {err(status); return status;} \
		}

	Print(L"Bootloader started\r\n");

	EFI_HANDLE device_handle;
	EFI_ASSERT(get_device_handle(ImageHandle, &device_handle));

	void * buffer;
	uint32_t kernel_file_size;
	EFI_ASSERT(load_kernel_file(ImageHandle, device_handle, &buffer, &kernel_file_size));

	uint32_t pages_needed;
	if(validate_kernel_elf(&pages_needed, buffer, kernel_file_size)) {
		Print(L"Invalid/corrupt kernel file\r\n");
		sleep_3_seconds();
		return 1;
	};
	Print(L"Validated kernel executable\r\n");



	UINTN num_memory_descriptors, memory_map_key, memory_descriptor_size;
	UINT32 memory_descriptor_version;
	void * memory_descriptors;

	// // Data is in EfiLoaderData memory
	memory_descriptors = LibMemoryMap (&num_memory_descriptors, &memory_map_key, 
		&memory_descriptor_size, &memory_descriptor_version);

	if(!memory_descriptors) {
		return EFI_OUT_OF_RESOURCES;
	}

	uint64_t free_memory_available = 0;
	EFI_MEMORY_DESCRIPTOR * desc = memory_descriptors;
	for(unsigned int i = 0; i < num_memory_descriptors; i++) {
		if(desc->Type == EfiConventionalMemory || desc->Type == EfiBootServicesCode || desc->Type == EfiBootServicesData
			|| desc->Type == EfiLoaderCode || desc->Type == EfiLoaderData)
		{
			free_memory_available += desc->NumberOfPages;
		}

		desc = (EFI_MEMORY_DESCRIPTOR *)((uintptr_t)desc + memory_descriptor_size);
	}
	free_memory_available *= 4096ull;
	Print(L"Free memory: %lu\n", free_memory_available);


	// Map kernel & first 512GiB

	void * kernel_0_pages = NULL;
	EFI_ASSERT(uefi_call_wrapper((void*)BS->AllocatePages, 4, AllocateAnyPages, EfiLoaderData, pages_needed, &kernel_0_pages));

	memclr64(kernel_0_pages, pages_needed * 4096/8);
	Print(L"Allocated and zeroed kernel pages\r\n");


	setup_paging(buffer, kernel_0_pages);
	Print(L"Switched page tables\r\n");


	Print(L"Press any key to exit boot services\r\n");

	// Clear input buffers
	EFI_ASSERT(uefi_call_wrapper((void*)ST->ConIn->Reset, 2, ST->ConIn, TRUE)); 

	// Wait for keystroke
	Pause();

	Print(L"Starting kernel\r\n");

	memory_descriptors = LibMemoryMap (&num_memory_descriptors, &memory_map_key, 
		&memory_descriptor_size, &memory_descriptor_version);
	if(!memory_descriptors) {
		return EFI_OUT_OF_RESOURCES;
	}
	EFI_ASSERT(uefi_call_wrapper((void*)BS->ExitBootServices, 2, ImageHandle, memory_map_key));

	kernel_main(ImageHandle, SystemTable);

	return 1;
}
