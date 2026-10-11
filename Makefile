#
# IotaOS - Makefile
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

BUILD_DIR = ./build
SRC_DIR = ./src
APPS_DIR = $(SRC_DIR)/apps
ADK_DIR = $(SRC_DIR)/IotaADK

# Toolchain
CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-ld
AR = i686-elf-ar
# Flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -I$(SRC_DIR)/kernel

# Kernel Sources
C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)
ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# App Discovery
ALL_APP_FILES = $(wildcard $(APPS_DIR)/*.s) $(wildcard $(APPS_DIR)/*.c)
APP_SOURCES = $(filter-out $(APPS_DIR)/crt0.s, $(ALL_APP_FILES))

LIBC_DIR = src/IotaADK/libc
LIBC_SOURCES = $(wildcard $(LIBC_DIR)/src/*.c)
LIBC_OBJECTS = $(patsubst $(LIBC_DIR)/src/%.c, $(BUILD_DIR)/libc_%.o, $(LIBC_SOURCES))

# CHANGED: Output .elf extensions instead of .ib
APP_BINARIES = $(patsubst $(APPS_DIR)/%.s, $(BUILD_DIR)/initrd_root/%.elf, \
		$(patsubst $(APPS_DIR)/%.c, $(BUILD_DIR)/initrd_root/%.elf, $(APP_SOURCES)))

all: $(BUILD_DIR)/IotaOS $(BUILD_DIR)/libc.a $(APP_BINARIES)

# --- KERNEL BUILD ---
$(BUILD_DIR)/IotaOS: $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(ALL_OBJECTS) -lgcc


# --- LIBC BUILD ---

$(BUILD_DIR)/libc.a: $(LIBC_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	@echo "Archiving libc.a..."
	$(AR) rcs $@ $^

$(BUILD_DIR)/libc_%.o: $(LIBC_DIR)/src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(LIBC_DIR)/include -Isrc/IotaADK/include

# --- C APP BUILD ---
# CHANGED: Target .elf and link directly into the ramdisk directory
$(BUILD_DIR)/initrd_root/%.elf: $(APPS_DIR)/%.c $(ADK_DIR)/lib/crt0.s $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/libc.a
	@mkdir -p $(BUILD_DIR)/initrd_root
	@echo "Building C App using ADK: $<"
	$(AS) $(ADK_DIR)/lib/crt0.s -o $(BUILD_DIR)/crt0.o
	$(CC) -c $< -o $(BUILD_DIR)/$*.o $(CFLAGS) -I$(ADK_DIR)/include -I$(LIBC_DIR)/include -fno-asynchronous-unwind-tables
	$(LD) -m elf_i386 -T $(ADK_DIR)/lib/linker.ld -o $@ $(BUILD_DIR)/crt0.o $(BUILD_DIR)/$*.o -L$(BUILD_DIR) -lc
run: all
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@mkdir -p $(BUILD_DIR)/initrd_root
	rm -f $(BUILD_DIR)/initrd_root/*.ib
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	cd $(BUILD_DIR)/initrd_root && tar --format=ustar -cf ../iso/boot/initrd.img *
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso

debug: all
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@mkdir -p $(BUILD_DIR)/initrd_root
	rm -f $(BUILD_DIR)/initrd_root/*.ib
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	cd $(BUILD_DIR)/initrd_root && tar --format=ustar -cf ../iso/boot/initrd.img *
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -serial stdio

sync:
	@chmod +x tools/sync_tasks.sh
	@./tools/sync_tasks.sh
	@echo "Local Taskwarrior Status:"
	@task project:IotaOS next

clean:
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)