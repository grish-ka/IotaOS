#
# IotaOS - Makefile
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

# Builds .ib applications for IotaOS

# Tools
CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-ld
OBJCOPY = i686-elf-objcopy

# Compiler Flags
# -I./include tells GCC to look in the local 'include' folder for iota.h
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -I./include -fno-asynchronous-unwind-tables

# Target App Name (Change this to build a different file!)
APP_NAME = hello

all: $(APP_NAME).ib

$(APP_NAME).ib: $(APP_NAME).c lib/crt0.s lib/linker.ld
	@echo "--- Compiling $(APP_NAME).ib ---"
	
	# 1. Compile the assembly bootstrapper (crt0.s)
	$(AS) lib/crt0.s -o crt0.o
	
	# 2. Compile the user's C code
	$(CC) -c $(APP_NAME).c -o $(APP_NAME).o $(CFLAGS)
	
	# 3. Link them together (crt0.o MUST be first!)
	$(LD) -m elf_i386 -T lib/linker.ld -o $(APP_NAME).elf crt0.o $(APP_NAME).o
	
	# 4. Strip the ELF data to make a raw binary (.ib)
	$(OBJCOPY) -O binary $(APP_NAME).elf $(APP_NAME).ib
	
	# 5. Clean up the messy intermediate files
	@rm -f crt0.o $(APP_NAME).o $(APP_NAME).elf
	
	@echo "Success! $(APP_NAME).ib is ready to be loaded into your Ramdisk."

clean:
	rm -f *.o *.elf *.ib