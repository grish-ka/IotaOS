 <!-- 
 IotaOS - README.md
 Copyright (c) 2026 grish-ka
 Licensed under the MIT License. 
 -->

# IotaOS
A simple OS made for fun 

## **🗺️ Roadmap**

IotaOS is actively evolving\! Here is the current development roadmap for upcoming releases:

### **🛠️ Phase 1: The Microkernel Shift (v0.2.x)**

* **Architecture Overhaul:** Transition from a monolithic kernel to a lightweight, modular **Microkernel**.  
* **IPC (Inter-Process Communication):** Build a secure message-passing system so user-space programs can safely talk to the kernel.  
* **Virtual Memory:** Implement Paging to give every .ib application its own isolated, secure memory space.

### **🔌 Phase 2: The Driver Ecosystem (v0.3.x)**

* **User-Space Drivers:** Create a dedicated drivers/ folder to run hardware controllers safely in user-space (Ring 3\) instead of kernel space (Ring 0).  
* **Storage & Filesystems:** Add IDE/ATA disk drivers and a read/write filesystem (like FAT32 or ext2) to replace the read-only ramdisk. Developers will be able to save files permanently\!  
* **Web Driver:** Begin groundwork for a web driver to handle basic networking and interface logic.

### **💿 Phase 3: Persistent Installation (v0.4.x)**

* **IotaOSInstaller.iso:** Develop a standalone, bootable setup program that partitions a hard drive and installs IotaOS permanently to a machine, moving beyond the "Live CD" experience.

### **📚 Phase 4: ADK Expansion**

* **Standard C Library (libc):** Implement a minimal standard library (adding functions like malloc, free, printf, fopen) for the Iota ADK to make building apps even easier.

**Current Phase:** Phase 1\! 🚀

### **📝 Active Development Checklist**

 - [ ] **Phase 1: Architecture & Microkernel**  
   - [x] Implement Hardware Timer (PIT) interrupts.  
   - [x] Build a basic Task Scheduler for context switching.  
   - [ ] Set up Paging (Virtual Memory) directory tables.  
   - [ ] Implement Ring 3 (User Mode) privilege switching.  
   - [ ] Write initial IPC (Inter-Process Communication) syscalls for message passing.  
 - [ ] **Phase 2: The Driver Ecosystem**  
   - [ ] Implement IDE/ATA driver for disk reading and writing.  
   - [ ] Write a read/write filesystem parser (e.g., FAT32 or ext2).  
   - [ ] Move PS/2 Keyboard driver out of the kernel and into User Space.  
   - [ ] Move VGA/Terminal driver out of the kernel and into User Space.  
   - [ ] Draft initial networking/Web Driver interface.  
 - [ ] **Phase 3: Persistent Installation**  
   - [ ] Create the standalone IotaOSInstaller.iso bootable image.  
   - [ ] Write disk partitioning and formatting logic.  
   - [ ] Implement file copying from the Live CD ramdisk to the physical hard drive.  
   - [ ] Configure GRUB installation for the target drive.  
 - [ ] **Phase 4: ADK Expansion**  
   - [ ] Port a minimal standard C library (libc headers like string.h, stdio.h).  
   - [ ] Implement dynamic memory allocation (malloc, free) in user space.  
   - [ ] Add file I/O wrappers (fopen, fread, fwrite, fclose).  
   - [ ] Provide detailed developer documentation and comprehensive example apps.

This is slowly being converted to `TODO.json` for [go-check](https://github.com/grish-ka/go-check) as we are using it so [go-check](https://github.com/grish-ka/go-check) will be getting updates

## Make your own `.ib` file
1. Download the ADK (App Development Kit) from the latest realese
2. Create the `Makefile` for building your app
```Make
# Point this to wherever the ADK is installed on the computer!
IOTA_ADK_PATH = ../IotaADK

all: myapp.ib

myapp.ib: main.c
	# We just use the tools from the IOTA_ADK_PATH variable!
	i686-elf-as $(IOTA_ADK_PATH)/lib/crt0.s -o crt0.o
	i686-elf-gcc -c main.c -o myapp.o -I$(IOTA_ADK_PATH)/include -fno-asynchronous-unwind-tables
	i686-elf-ld -T $(IOTA_ADK_PATH)/lib/linker.ld -o myapp.elf crt0.o myapp.o
	i686-elf-objcopy -O binary myapp.elf myapp.ib
```

## Set Up GCC Cross Compiler

> [!NOTE]
> I have tried this before and for me I have not needed build-essential (on Ubuntu) or @development-tools (on Fedora), so beware you may need gcc, g++, and make installed if they aren't already.

### **1\. Install Build Dependencies**

Choose your specific Linux distribution below to see the required commands:

<details>

<summary><b>🐧 Ubuntu / Debian</b> (Click to expand)</summary>

> [!IMPORTANT]
> If you have Ubuntu at a bare minimum installed then you need to run this or install build essantials
```bash
# This ensures you have awk, sed, make, and gcc
sudo apt install build-essentials
```

```bash
sudo apt update  
sudo apt install bison flex libgmp3-dev libmpc-dev libmpfr-dev libisl-dev xorriso grub-pc-bin 

# For Texinfo (optional)  
sudo apt install libncurses5-dev libncursesw5-dev
```
</details>

<details>

<summary><b>🎩 Fedora</b> (Click to expand)</summary>

> [!IMPORTANT]
> If you have Fedora at a bare minimum installed then you need to run this or install build essantials
```bash
# This group ensures you have awk, sed, make, and gcc
sudo dnf groupinstall "Development Tools"
```

```bash
sudo dnf install bison flex gmp-devel libmpc-devel mpfr-devel isl-devel xorriso grub2-pc-modules grub2-tools-extra
```

# For Texinfo (optional)  
sudo dnf install ncurses-devel

</details>
2. Install Legacy Texinfo (Optional)

> [!CAUTION]
> I would **definitely not** recommend manually installing an *ancient* version of `texinfo` like I am. I’m only doing this for the sake of learning the deep internals and following the OSDev Wiki to the letter. If you just want to build the OS, don't install `texinfo`! However, if you plan on changing core components, I would recommend it; it is a fairly fast install and might save time if you need to look up documentation for a feature you aren't familiar with.

2a. Install Texinfo (4.13)

```bash
mkdir -p ~/src
cd ~/src

# Download and unpack
wget https://ftp.gnu.org/gnu/texinfo/texinfo-4.13a.tar.gz
tar -xf texinfo-4.13a.tar.gz
cd texinfo-4.13

# Fix modern compiler security errors
sed -i 's/_GL_WARN_ON_USE (gets, "gets is a security hole - use fgets instead");/\/\/_GL_WARN_ON_USE (gets, "gets is a security hole - use fgets instead");/g' gnulib/lib/stdio.in.h

# Build and link against ncurses
./configure --prefix=$HOME/opt/texinfo4 LDFLAGS="-lncurses"
make
make install

# Add to path
export PATH="$HOME/opt/texinfo4/bin:$PATH"
```

2b. Verify Installation
```bash
makeinfo --version       # Should report 4.13
```

3. Install Binutils (2.23.2)

3a. Setup enviroment
> [!IMPORTANT]
> These variables are session-based. If you close your terminal, you must run these again before proceeding.
```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```

3b. Install Binutils (2.23.2)
```bash
cd ~/src
wget https://ftp.gnu.org/gnu/binutils/binutils-2.23.2.tar.gz
tar -xf binutils-2.23.2.tar.gz

mkdir build-binutils
cd build-binutils

../binutils-2.23.2/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror MAKEINFO=missing
make MAKEINFO=missing
make install MAKEINFO=missing
```

4. Install GCC (4.8.0)

```bash
cd ~/src
wget https://ftp.gnu.org/gnu/gcc/gcc-4.8.0/gcc-4.8.0.tar.gz
tar -xf gcc-4.8.0.tar.gz

mkdir build-gcc
cd build-gcc

# Configure for C and C++
../gcc-4.8.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers MAKEINFO=missing

# GCC build has two main targets: the compiler and the internal library
make all-gcc CXXFLAGS="-O2 -std=gnu++03" MAKEINFO=missing
make all-target-libgcc MAKEINFO=missing
make install-gcc CXXFLAGS="-O2 -std=gnu++03" MAKEINFO=missing
make install-target-libgcc MAKEINFO=missing
```
5. Verification <br>
Go to the repo root and run these:
```bash
i686-elf-gcc --version   # Should report 4.8.0
i686-elf-ld --version    # Should report 2.23.2
i686-elf-as --version    # Should report 2.23.2
```