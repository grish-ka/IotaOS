BUILD_DIR = ./build
SRC_DIR = ./src

# 1. Automatically find all .c and .s files (Notice drivers are inside kernel/)
C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

# 2. Automatically convert the lists of .c and .s files into a list of .o files
C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)

# 3. Combine them into one master list of everything that needs to be linked
ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Compiler flags (-I$(SRC_DIR)/kernel allows #include "drivers/terminal.h" to work)
CC = i686-elf-gcc
AS = i686-elf-as
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I$(SRC_DIR)/kernel

# The default rule
all: $(BUILD_DIR)/IotaOS

# 4. The Linker Rule (Now uses $(ALL_OBJECTS) so it never forgets a driver!)
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
	@# 1. Create a clean, dedicated staging area
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	
	@# 2. Copy the kernel to the 'boot' folder inside that area
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	
	@# 3. Copy your grub.cfg to the correct location
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	
	@# 4. Point grub-mkrescue ONLY at the 'iso' folder, not the whole 'build' folder
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso
	
	@echo "Booting QEMU..."
	qemu-system-i386 -cdrom $(BUILD_DIR)/IotaOS.iso

# 5. PATTERN RULES: The "Auto-Cookers"
# Tell make: "If you need a .o file, find the matching .c file and run this:"
%.o: %.c
	@echo "Compiling $<..."
	$(CC) -c $< -o $@ $(CFLAGS)

# Tell make: "If you need a .o file, find the matching .s file and run this:"
%.o: %.s
	@echo "Assembling $<..."
	$(AS) $< -o $@

clean:
	@echo "Cleaning up..."
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)/IotaOS $(BUILD_DIR)/IotaOS.iso
	rm -rf $(BUILD_DIR)