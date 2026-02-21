# IotaOS - Makefile (Fixed for App Automation)
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.

BUILD_DIR = ./build
SRC_DIR = ./src
APPS_DIR = $(SRC_DIR)/apps

C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

# Automated App Discovery
APP_SOURCES = $(wildcard $(APPS_DIR)/*.s)
APP_BINARIES = $(patsubst $(APPS_DIR)/%.s, $(BUILD_DIR)/initrd_root/%.ib, $(APP_SOURCES))

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)
ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

CC = i686-elf-gcc
AS = i686-elf-as
OBJCOPY = i686-elf-objcopy
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(SRC_DIR)/kernel

all: $(BUILD_DIR)/IotaOS $(APP_BINARIES)

$(BUILD_DIR)/IotaOS: $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(ALL_OBJECTS) -lgcc

# THE AUTOMATED APP RULE
$(BUILD_DIR)/initrd_root/%.ib: $(APPS_DIR)/%.s
	@mkdir -p $(BUILD_DIR)/initrd_root
	$(AS) $< -o $(BUILD_DIR)/$*.o
	$(CC) -Ttext 0 -nostdlib -o $(BUILD_DIR)/$*.elf $(BUILD_DIR)/$*.o
	$(OBJCOPY) -O binary $(BUILD_DIR)/$*.elf $@
	@rm $(BUILD_DIR)/$*.o $(BUILD_DIR)/$*.elf

run: all
	@echo "Building ISO..."
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@mkdir -p $(BUILD_DIR)/initrd_root
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	
	@echo "Current files in initrd_root:"
	@ls $(BUILD_DIR)/initrd_root
	
	@echo "Packing Ramdisk..."
	@echo "IotaOS Ramdisk Root" > $(BUILD_DIR)/initrd_root/manifest.txt
	# We name the output .img, but it's still a tar archive internally
	cd $(BUILD_DIR)/initrd_root && tar -cf ../iso/boot/initrd.img *
	
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	
	@echo "Booting QEMU..."
	qemu-system-i386 -cdrom $(BUILD_DIR)/IotaOS.iso

clean:
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)