BUILD_DIR = ./build
SRC_DIR = ./src

all: bootloader kernel
	@echo "Linking bootloader and kernel..."
	@mkdir -p $(BUILD_DIR)
	i686-elf-gcc -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(SRC_DIR)/bootloader/boot.o $(SRC_DIR)/kernel/kernel.o -lgcc
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

bootloader: $(SRC_DIR)/bootloader/boot.s
	@echo "Assembling bootloader..."
	i686-elf-as $(SRC_DIR)/bootloader/boot.s -o $(SRC_DIR)/bootloader/boot.o

kernel: $(SRC_DIR)/kernel/kernel.c
	@echo "Compiling kernel..."
	i686-elf-gcc -c $(SRC_DIR)/kernel/kernel.c -o $(SRC_DIR)/kernel/kernel.o -std=gnu99 -ffreestanding -O2 -Wall -Wextra

clean:
	@echo "Cleaning up..."
	find $(SRC_DIR) -type f -name "*.o" -delete
	rm -rf $(BUILD_DIR)