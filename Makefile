# 
# IotaOS - Makefile
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

BUILD_DIR = ./build
SRC_DIR = ./src

C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)

ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

CC = i686-elf-gcc
AS = i686-elf-as
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(SRC_DIR)/kernel

all: $(BUILD_DIR)/IotaOS

$(BUILD_DIR)/IotaOS: $(ALL_OBJECTS)
	@echo "Linking bootloader, kernel, and drivers..."
	@mkdir -p $(BUILD_DIR)
	$(CC) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(ALL_OBJECTS) -lgcc
	@if grub-file --is-x86-multiboot $(BUILD_DIR)/IotaOS; then \
		echo "multiboot confirmed"; \
	else \
		echo "the file is not multiboot"; \
		exit 1; \
	fi

run: all
	@echo "Building ISO..."
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@mkdir -p $(BUILD_DIR)/initrd_root
	
	@# Copy the kernel
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	
	@# --- CREATE THE UBUNTU-STYLE INITRD ---
	@echo "Creating test files for the Ramdisk..."
	@echo "This is the first file!" > $(BUILD_DIR)/initrd_root/file1.txt
	@echo "IotaOS filesystem is working!" > $(BUILD_DIR)/initrd_root/message.txt
	
	@# Pack the folder into an .img file inside the ISO's boot folder
	cd $(BUILD_DIR)/initrd_root && tar -cf ../iso/boot/initrd.img *
	
	@# Copy GRUB config
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	
	@# Build the ISO
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	
	@echo "Booting QEMU..."
	qemu-system-i386 -cdrom $(BUILD_DIR)/IotaOS.iso

%.o: %.c
	@echo "Compiling $<..."
	$(CC) -c $< -o $@ $(CFLAGS)

%.o: %.s
	@echo "Assembling $<..."
	$(AS) $< -o $@

clean:
	@echo "Cleaning up..."
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)/IotaOS $(BUILD_DIR)/IotaOS.iso
	rm -rf $(BUILD_DIR)