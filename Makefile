# IotaOS - Makefile (Fixed for App Automation)
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.

# Directories
BUILD_DIR = ./build
SRC_DIR = ./src
APPS_DIR = $(SRC_DIR)/apps

# Toolchain
CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# Flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(SRC_DIR)/kernel

# Kernel Sources
C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)
ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# App Discovery (Filters out crt0.s so it doesn't try to build it as a standalone app)
ALL_APP_FILES = $(wildcard $(APPS_DIR)/*.s) $(wildcard $(APPS_DIR)/*.c)
APP_SOURCES = $(filter-out $(APPS_DIR)/crt0.s, $(ALL_APP_FILES))
APP_BINARIES = $(patsubst $(APPS_DIR)/%.s, $(BUILD_DIR)/initrd_root/%.ib, \
               $(patsubst $(APPS_DIR)/%.c, $(BUILD_DIR)/initrd_root/%.ib, $(APP_SOURCES)))

all: $(BUILD_DIR)/IotaOS $(APP_BINARIES)

# --- KERNEL BUILD ---
$(BUILD_DIR)/IotaOS: $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(ALL_OBJECTS) -lgcc

$(BUILD_DIR)/initrd_root/%.ib: $(APPS_DIR)/%.c $(APPS_DIR)/crt0.s $(APPS_DIR)/linker_app.ld
	@mkdir -p $(BUILD_DIR)/initrd_root
	@echo "Hard-linking C App: $<"
	# 1. Compile assembly
	$(AS) $(APPS_DIR)/crt0.s -o $(BUILD_DIR)/crt0.o
	# 2. Compile C (Notice we use -fno-asynchronous-unwind-tables to stop those 0x70 junk bytes)
	$(CC) -c $< -o $(BUILD_DIR)/$*.o $(CFLAGS) -fno-asynchronous-unwind-tables
	
	# Order is everything: crt0.o MUST be first
	$(LD) -m elf_i386 -T $(APPS_DIR)/linker_app.ld -o $(BUILD_DIR)/$*.elf \
		$(BUILD_DIR)/crt0.o \
		$(BUILD_DIR)/$*.o
	
	# 4. BINARY: Create the final file
	$(OBJCOPY) -O binary $(BUILD_DIR)/$*.elf $@
	
	@python3 -c "import sys; f=open('$@', 'rb'); b=f.read(2); \
	if b != b'IB': print('\n\033[31m[BUILD ERROR] Magic Number Missing in $@! Found:', b.hex(), '\033[0m\n'); sys.exit(1); \
	else: print('\033[32m[OK] Header Verified: $@\033[0m')"

# --- ASM APP BUILD ---
$(BUILD_DIR)/initrd_root/%.ib: $(APPS_DIR)/%.s
	@mkdir -p $(BUILD_DIR)/initrd_root
	@echo "Assembling ASM App: $<"
	$(AS) $< -o $(BUILD_DIR)/$*.o
	$(LD) -m elf_i386 -Ttext 0 -o $(BUILD_DIR)/$*.elf $(BUILD_DIR)/$*.o
	$(OBJCOPY) -O binary $(BUILD_DIR)/$*.elf $@
	@rm -f $(BUILD_DIR)/$*.o $(BUILD_DIR)/$*.elf

run: all
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	@mkdir -p $(BUILD_DIR)/initrd_root
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	# Pack the Ramdisk (TAR format)
	cd $(BUILD_DIR)/initrd_root && tar -cf ../iso/boot/initrd.img *
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	qemu-system-i386 -cdrom $(BUILD_DIR)/IotaOS.iso

clean:
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)