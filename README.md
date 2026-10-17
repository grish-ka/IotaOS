<!-- 
IotaOS - README.md
Copyright (c) 2026 grish-ka
Licensed under the MIT License. 
-->

# IotaOS

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Target Architecture](https://img.shields.io/badge/Arch-i686%20(x86)-orange.svg)](https://github.com/grish-ka/IotaOS)
[![Version](https://img.shields.io/badge/Version-v1.0.1-brightgreen.svg)](https://github.com/grish-ka/IotaOS/releases)
[![Build Status](https://img.shields.io/github/actions/workflow/status/grish-ka/IotaOS/release.yml?branch=main&label=Build%20%26%20Release)](https://github.com/grish-ka/IotaOS/actions)

[![IotaOS Components](https://go-pkg.vercel.app/api?project=IotaOS&data=kernel%3A1.0.1-u%2Cbash%3A5.2.21-u%2Ccoreutils%3A9.5-u%2Cmandoc%3A1.14.6-u%2Cncurses%3A6.5-u)](https://github.com/grish-ka/IotaOS)

**IotaOS** is a 32-bit x86 multitasking Unix-like operating system designed for simplicity, modularity, and practical experimentation. Featuring a custom monolithic kernel with a POSIX-compatible system call layer, IotaOS boots directly into an interactive **GNU Bash** shell with a ported suite of standard GNU utilities, a standard C library (**IotaADK libc**), an in-memory Virtual File System, ANSI terminal emulation, and an interactive boot-time diagnostics system.

---

## 🌟 Key Features

### 🖥️ Kernel & Architecture
- **32-Bit Protected Mode:** Booted cleanly via GRUB and the Multiboot specification.
- **Preemptive Multitasking:** Round-robin task scheduler driven by a 100 Hz Programmable Interval Timer (PIT).
- **Virtual Memory & Paging:** Two-level x86 paging with isolated per-process virtual address spaces, user/kernel memory protection, and dynamic process heap (`brk` / `mmap`).
- **POSIX Process Model:** Full implementation of `fork()`, `execve()`, `waitpid()`, `exit()`, process IDs (`PID`/`PPID`), session leadership, signals, and command-line arguments (`argc`/`argv`) and environment variables (`environ`).
- **Rich Syscall Interface:** 40+ system calls dispatched via `int 0x80`, providing standard file I/O, directory traversal, piping, process control, and terminal manipulation.
- **Virtual File System (VFS):** In-memory writable filesystem populated at boot from a `ustar` initrd ramdisk, supporting permissions, sticky bits (`/tmp` 1777), `/dev` character devices, and user directories (`/home`, `/root`).
- **Device & TTY Subsystem:** Line-discipline terminal subsystem (`/dev/tty1`, `/dev/console`), `/dev/null`, `/dev/zero`, and CMOS Real-Time Clock (`/dev/rtc`).
- **ANSI / VT100 Terminal Emulation:** 80×25 VGA text driver supporting 16 colors, cursor positioning, and standard ANSI escape sequences.
- **Boot Diagnostics Console:** Pause during boot by pressing <kbd>DEL</kbd>, <kbd>ESC</kbd>, or <kbd>F2</kbd> to inspect CPUID features, memory maps, RTC time, and PIC interrupt masks.
- **Kernel Crash Screen:** Diagnostic Blue Screen of Death (BSOD) displaying CPU registers, stack dumps, and exception details on fatal panics.

### 📦 Ported GNU Userspace
IotaOS ports real, unmodified upstream GNU and Unix utilities:
- **Shell:** GNU Bash 5.2.21 (primary shell, symlinked to `/bin/sh`) & built-in `iosh` fallback.
- **Core Utilities:** GNU Coreutils 9.5 (60+ tools including `cat`, `ls`, `cp`, `mv`, `rm`, `mkdir`, `rmdir`, `chmod`, `chown`, `chgrp`, `touch`, `stat`, `date`, `echo`, `env`, `head`, `tail`, `sort`, `uniq`, `wc`, `tr`, `cut`, `tee`, `seq`, `sleep`, `test`, `uname`, `whoami`, `yes`, `dd`, `df`, `du`, `ln`, `pwd`, `sync`).
- **Search & Filters:** GNU Grep 3.11, GNU Sed 4.9, GNU Gawk 5.3.0 (`awk`), and GNU Findutils 4.10.0 (`find`, `xargs`).
- **File Comparison:** GNU Diffutils 3.10 (`diff`, `cmp`, `diff3`, `sdiff`).
- **Compression & Archives:** GNU Gzip 1.13 (`gzip`, `gunzip`, `zcat`), GNU Tar 1.35, and zlib 1.3.1.
- **Pagers & Terminal Utilities:** Ncurses 6.5, Less 668, and util-linux `more`.
- **System Documentation:** Mandoc 1.14.6 (`man`, `apropos`, `whatis`, `makewhatis`) with pre-indexed offline manual pages in `/usr/share/man`.

### 🧰 IotaADK (Application Development Kit)
- **Standard C Library (`libc.a`):** Freestanding C runtime providing `stdio.h` (`printf`, `scanf`, file streams), `stdlib.h` (`malloc`, `free`, `strtol`), `string.h`, `unistd.h`, `fcntl.h`, `dirent.h`, `termios.h`, `math.h`, `regex.h`, `glob.h`, `getopt.h`, and more.
- **Compiler Driver (`tools/iota-cc`):** Drop-in compiler wrapper configuring include paths, start-up stubs (`crt0.o`), and linker scripts (`linker.ld`).
- **Autotools Porting Framework:** `tools/build-port.sh` and pre-seeded autoconf cache (`tools/iota-config.site`) for building `./configure`-based third-party packages.

---

## 🚀 Quick Start (Running Prebuilt ISO)

You can grab the latest bootable ISO (`IotaOS.iso`) from the [GitHub Releases](https://github.com/grish-ka/IotaOS/releases) page.

To launch IotaOS in QEMU:
```bash
qemu-system-i386 -m 512M -cdrom IotaOS.iso
```

> [!TIP]
> During boot, look for the diagnostics prompt: press <kbd>DEL</kbd>, <kbd>ESC</kbd>, or <kbd>F2</kbd> to enter the boot diagnostic screen. Press <kbd>ENTER</kbd> when ready to proceed into Bash!

---

## 🛠️ Building from Source

### 1. Install Prerequisites

Choose your Linux distribution:

<details>
<summary><b>🐧 Ubuntu / Debian</b> (Click to expand)</summary>

```bash
sudo apt update
sudo apt install -y build-essential bison flex xorriso grub-pc-bin mtools zip zlib1g-dev qemu-system-x86 wget
```
</details>

<details>
<summary><b>🎩 Fedora</b> (Click to expand)</summary>

```bash
sudo dnf install -y @development-tools bison flex xorriso grub2-pc-modules grub2-tools-extra mtools zip zlib-devel qemu-system-x86 wget
```
</details>

<details>
<summary><b>🏹 Arch Linux</b> (Click to expand)</summary>

```bash
sudo pacman -S --needed base-devel bison flex xorriso grub mtools zip zlib qemu-system-x86 wget
```
</details>

### 2. Set Up the `i686-elf` Cross-Compiler

A dedicated `i686-elf` GCC toolchain is required to build the kernel and userland binaries cleanly.

#### Option A: Pre-compiled Toolchain (Recommended, ~30 seconds)
You can download a pre-built toolchain matching the project's CI:

```bash
mkdir -p ~/opt
wget https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/i686-elf-tools-linux.zip
unzip -q i686-elf-tools-linux.zip -d ~/opt/i686-elf-tools
rm i686-elf-tools-linux.zip

# Add the toolchain to your PATH (add this to ~/.bashrc or ~/.profile to persist)
export PATH="$HOME/opt/i686-elf-tools/bin:$PATH"
```

Verify your toolchain:
```bash
i686-elf-gcc --version
i686-elf-ld --version
```

<details>
<summary><b>Option B: Building Cross-Compiler from Source (OSDev Guide)</b> (Click to expand)</summary>

If you prefer building your own toolchain from upstream sources:

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
mkdir -p ~/src && cd ~/src

# 1. Binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz
tar -xf binutils-2.42.tar.xz
mkdir build-binutils && cd build-binutils
../binutils-2.42/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$(nproc)
make install
cd ~/src

# 2. GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.xz
tar -xf gcc-13.2.0.tar.xz
mkdir build-gcc && cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c,c++ --without-headers
make all-gcc -j$(nproc)
make all-target-libgcc -j$(nproc)
make install-gcc
make install-target-libgcc
```
</details>

---

### 3. Build the OS and ISO

From the root of the repository:

```bash
# Build the bootable ISO with GNU Bash and all ported GNU utilities
make iso WITH_BASH=y
```

> [!NOTE]
> On the first build, `make iso` automatically fetches, configures, and cross-compiles upstream tarballs for Bash, Coreutils, Findutils, Sed, Grep, Gawk, Diffutils, Gzip, Tar, Less, Ncurses, and Mandoc. The sources are cached under `src/gnu/`.

### 4. Running & Debugging

| Target | Description |
| :--- | :--- |
| `make run` | Boots `IotaOS.iso` in QEMU with 4 GB allocated RAM. |
| `make debug` | Boots in QEMU with serial output redirected to stdio (`-serial stdio`). |
| `make pause` | Starts QEMU in a frozen state with monitor attached (`-S -monitor stdio`). Use `c` to resume execution or `info registers` to inspect state. |
| `make clean` | Cleans object files, compiled binaries, and ISO image. |

---

## 💻 Creating Apps with IotaADK

IotaOS comes with the **Iota Application Development Kit (IotaADK)**, allowing you to develop freestanding C programs that run directly on IotaOS using standard POSIX APIs or native system calls.

### Minimal Example: `hello.c`

```c
#include <stdio.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("Hello from IotaOS user-space!\n");
    if (argc > 1) {
        printf("First argument: %s\n", argv[1]);
    }
    return 0;
}
```

### Compiling with `tools/iota-cc`

The repository provides `tools/iota-cc`, an automated compiler wrapper that applies all required flags, includes project headers, and links `crt0.o`, `libc.a`, and the linker script:

```bash
# Compile and link using the wrapper driver
./tools/iota-cc hello.c -o hello

# Or using standard Makefile workflow:
i686-elf-gcc -c hello.c -o hello.o -Isrc/IotaADK/libc/include -Isrc/IotaADK/include -ffreestanding -fno-pie -fno-pic
i686-elf-ld -m elf_i386 -T src/IotaADK/lib/linker.ld build/crt0.o hello.o -Lbuild -lc -o hello
```

---

## 🏗️ Porting Third-Party Packages

IotaOS includes an extensible porting subsystem under `tools/`:

- **`tools/iota-cc`**: Compiler wrapper acting as a drop-in `CC` for autoconf scripts.
- **`tools/iota-config.site`**: Pre-seeded autoconf cache answering target architecture questions (sizes, types, function availability) to avoid running test binaries during cross-compilation.
- **`tools/build-port.sh`**: Generic script to fetch, unpack, configure, compile, and strip any `./configure`-based package.
- **`tools/build-ports.sh`**: Master driver script that builds the complete userspace utility set.

To port a new GNU or Unix tool:
```bash
tools/build-port.sh <pkg-name> <version> <tarball-url> "<configure-args>" <bin-relative-path>
```

---

## 📁 Repository Structure

```text
IotaOS/
├── .github/workflows/       # Continuous integration & automated release workflow
├── grub.cfg                 # GRUB bootloader configuration
├── Makefile                 # Top-level build automation
├── rootfs/                  # Root filesystem skeleton (/etc, /root, /home, /usr/share/man)
├── src/
│   ├── bootloader/          # Multiboot entry & early assembly (boot.s)
│   ├── kernel/              # Core monolithic kernel
│   │   ├── cpu/             # IDT, ISRs, 8259 PIC, multitasking scheduler, syscall dispatcher
│   │   ├── drivers/         # VGA terminal, ANSI parser, keyboard, RTC, timer, diagnostics, ELF loader
│   │   ├── fs/              # Virtual File System (VFS), tar ramdisk parser, file table
│   │   └── mem/             # Physical Memory Manager (PMM) & Virtual Memory / Paging
│   ├── IotaADK/             # App Development Kit
│   │   ├── include/         # Kernel syscall headers (iota.h)
│   │   ├── lib/             # Application entrypoint (crt0.s) and linker script (linker.ld)
│   │   └── libc/            # Standard C library implementation (stdio, stdlib, string, etc.)
│   └── apps/                # Built-in demo & diagnostic programs (iosh, meminfo, tests, bsod)
└── tools/                   # Porting engine, cross-compiler wrappers, and maintenance scripts
```

---

## 🗺️ Roadmap

With the **v1.0** milestone reached, IotaOS has successfully transitioned from an initial prototype to a multitasking operating system with a rich GNU userspace. Upcoming development goals include:

### 💾 Storage & Filesystems (v1.1)
- [ ] IDE/ATA and AHCI disk controller drivers for physical hard drive access.
- [ ] Read/write filesystem implementation (ext2 or FAT32) to replace ramdisk-only storage.
- [ ] Partition table parsing (MBR / GPT).
- [ ] Standalone installer image (`IotaOSInstaller.iso`) for permanent disk installations.

### 🌐 Networking (v1.2)
- [ ] Network Interface Card (NIC) drivers (e.g. Realtek RTL8139 and Intel e1000).
- [ ] Kernel network packet buffer architecture (`sk_buff` style).
- [ ] Basic TCP/IP network stack (ARP, IPv4, ICMP ping, UDP, TCP).
- [ ] Socket syscalls (`sys_socket`, `sys_connect`, `sys_bind`, `sys_listen`, `sys_accept`).

### ⚙️ Kernel Enhancements & Future Architecture
- [ ] Dynamic linking and shared object (`.so`) loader.
- [ ] User-space device driver framework experimentation.
- [ ] Ported text editor (nano or vim) and self-hosted C compiler.

---

## 📄 License

IotaOS is released under the [MIT License](LICENSE).  
Copyright &copy; 2026 grish-ka.