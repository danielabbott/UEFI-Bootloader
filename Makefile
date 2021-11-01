
# -- C CONFIG --

CC = clang
ASSEMBLER = nasm


CFLAGS_COMMON = -c -std=c11 -MMD -Wall -Wextra \
-Wshadow -Wno-missing-field-initializers -Werror=implicit-function-declaration \
-Wmissing-prototypes -Wimplicit-fallthrough \
-Wunused-macros -Wcast-align -Werror=incompatible-pointer-types \
-Wformat-security -Wundef -Werror=unused-result \
-Werror=int-conversion \
-Iconfig_parser -fstack-protector -I pigeon_engine/include -isystem deps


ifeq ($(CC), clang)
CFLAGS_COMMON += -Wshorten-64-to-32 -Wconditional-uninitialized -Wimplicit-int-conversion \
-Wimplicit-float-conversion -Wimplicit-int-float-conversion -Wno-newline-eof -Wconversion
else
CFLAGS_COMMON += -Wno-sign-conversion
endif

CFLAGS_KERNEL = $(CFLAGS_COMMON) -ffreestanding -mcmodel=kernel -mno-red-zone -mno-mmx -mno-sse -mno-sse2


CFLAGS_EFI_BOOTLOADER = $(CFLAGS_COMMON) -I/usr/include/efi/x86_64 -std=c11 -DEFI_FUNCTION_WRAPPER -fpic \
-ffreestanding -fno-stack-protector -fno-stack-check -fshort-wchar -mno-red-zone -mno-mmx -mno-sse -mno-sse2

BUILD_DIR = build


SOURCES_KERNEL = $(wildcard kernel/*.c)
SOURCES_KERNEL_ASM = $(wildcard kernel/*.asm)
OBJECTS_KERNEL = $(SOURCES_KERNEL:%.c=$(BUILD_DIR)/%.o) $(SOURCES_KERNEL_ASM:%=$(BUILD_DIR)/%.o)

SOURCES_EFI_BOOTLOADER = $(wildcard efi_bootloader/*.c)
SOURCES_EFI_BOOTLOADER_ASM = $(wildcard efi_bootloader/*.asm)
OBJECTS_EFI_BOOTLOADER = $(SOURCES_EFI_BOOTLOADER:%.c=$(BUILD_DIR)/%.o) $(SOURCES_EFI_BOOTLOADER_ASM:%=$(BUILD_DIR)/%.o)

DEPS = $(OBJECTS_KERNEL:%.o=%.d) $(OBJECTS_EFI_BOOTLOADER:%.o=%.d)


all: disk_contents/efi/EFI/BOOT/BOOTX64.EFI disk_contents/efi/kernel

$(BUILD_DIR)/kernel/%.o: kernel/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_KERNEL) $< -o $@

$(BUILD_DIR)/kernel/%.asm.o: kernel/%.asm
	@mkdir -p $(@D)
	$(ASSEMBLER) -w+orphan-labels -f elf64 $< -o $@

$(BUILD_DIR)/efi_bootloader/%.o: efi_bootloader/%.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS_EFI_BOOTLOADER) $< -o $@

$(BUILD_DIR)/efi_bootloader/%.asm.o: efi_bootloader/%.asm
	@mkdir -p $(@D)
	$(ASSEMBLER) -w+orphan-labels -f elf64 $< -o $@


-include $(DEPS)

disk_contents/efi/EFI/BOOT/BOOTX64.EFI: $(OBJECTS_EFI_BOOTLOADER) efi_bootloader/elf_x86_64_efi.lds
	ld -nostdlib -shared -Bsymbolic -T efi_bootloader/elf_x86_64_efi.lds /usr/lib/crt0-efi-x86_64.o $(OBJECTS_EFI_BOOTLOADER)  -o $(BUILD_DIR)/efi_bootloader/bootloader.so -L/usr/lib -lgnuefi -lefi

	mkdir -p $(@D)

	objcopy -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $(BUILD_DIR)/efi_bootloader/bootloader.so $@


disk_contents/efi/kernel: $(OBJECTS_KERNEL) kernel/link.ld
	ld.lld -T kernel/link.ld $(OBJECTS_KERNEL) -o $@ -nostdlib

clean:
	-rm -r $(BUILD_DIR)
	-rm disk_contents/efi/EFI/BOOT/BOOTX64.EFI disk_contents/efi/kernel