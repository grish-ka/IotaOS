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

[![IotaOS Components](https://go-pkg.vercel.app/api?project=IotaOS&data=kernel%3A1.0.1-u%2Cbash%3A5.2.21-n%2Ccoreutils%3A9.5-n%2Cmandoc%3A1.14.6-u%2Cncurses%3A6.5-n)](https://github.com/grish-ka/IotaOS)

**IotaOS** is a 32-bit x86 multitasking Unix-like operating system designed for simplicity, modularity, and high-performance direct-framebuffer computing. Featuring a custom monolithic kernel with a POSIX-compatible system call layer, IotaOS boots seamlessly through the **LightDM Graphical Display Manager** into an authentic, lightweight **XFCE 4 Desktop Environment** (running at 90+ FPS in software CPU emulation), an interactive **GNU Bash** shell, **DOOM 3D Direct**, **Wayland-Gears 3D**, or retro emulation, backed by persistent ATA storage and a rich GNU toolchain.

---

## 📊 System Feature Matrix

| Subsystem / Component | Implementation Status | Technical Details & Architecture | Launch Command / Interface |
| :--- | :---: | :--- | :--- |
| **LightDM Display Manager** | **Supported** | Graphical login manager on `/dev/fb0`, user switcher (`root`/`iota`), session picker, clock, power controls (reboot/shutdown) | `/bin/lightdm` (boot default) |
| **XFCE 4 Desktop Environment** | **Supported** | Dual-tier XFCE panels (Top panel + Floating dock), Whisker Menu, Workspaces 1 & 2, Thunar file manager, XFCE Terminal, XFWM4 window roll-up `[^]` | `/bin/xfce` |
| **High Refresh Rate Engine (90+ FPS)** | **Supported** | 60Hz, 75Hz, 120Hz, 144Hz & Uncapped modes; integer LUT geometry + dirty-rect presentation rendering at 90–120 FPS in QEMU | `/bin/xfce` / KSysGuard |
| **Wayland-Gears 3D Demo** | **Supported** | 3 interlocking gears (20T, 14T, 10T), directional 3D shading, 4 palettes, optimized integer LUT rasterizer (50–90 FPS) | `/bin/gears` (or XFCE window) |
| **DOOM 3D Raycasting Engine** | **Supported** | DDA texture raycaster, animated shotgun (muzzle flash & recoil), Doomguy status bar with glances, minimap, frags HUD | `/bin/doom` (or XFCE window) |
| **Retro CHIP-8 Emulator 2.0** | **Supported** | Full CHIP-8 virtual machine (600Hz), phosphor green CRT display, keyboard paddle input, built-in Pong & Maze ROMs | `/bin/emu` (or XFCE window) |
| **Persistent ATA / IDE HDD** | **Supported** | Primary ATA PIO block driver (`/dev/hda`), 512-byte sectors, cache flush; persists changes across reboots via `hda.img` | `/dev/hda` / `make run` |
| **Mouse & VM Calibration** | **Supported** | CPUID hypervisor detection (QEMU/KVM/VBox/VMware), 85% VM smoothing, 4 distinct cursors (Greybird, Target, Amber, Halo) | `/dev/mouse` / Settings |
| **Automated Test Suite (11/11)** | **Supported** | 11 automated test suites verifying Heap, VFS, Args, Env, ANSI, ATA HDD, Framebuffer, Mouse, Math, 32-bit PMM memory, and SB16 Audio (`/dev/dsp` + `/dev/mixer`) | `/bin/tests` |
| **GNU Bash Shell** | **Supported** | GNU Bash 5.2.21 with full job control, piping, history, environment variables, scripting | `/bin/bash` / `/bin/sh` |
| **GNU Coreutils & Tools** | **Supported** | Coreutils 9.5 (60+ tools), Grep 3.11, Sed 4.9, Gawk 5.3.0, Diffutils 3.10, Findutils 4.10.0, Mandoc 1.14.6 | `/bin/*` |
| **Linear Framebuffer Graphics** | **Supported** | VBE 1024×768 32bpp linear framebuffer (`/dev/fb0`), double-buffering, dirty-rect cursor blitting | `/dev/fb0` |
| **Physical Memory Manager** | **Supported** | BIOS/UEFI E820 memory mapping, 4 KB page frame allocator, supports up to 4 GB active physical RAM | `meminfo` |
| **POSIX System Calls** | **Supported** | 40+ system calls via `int 0x80`: `fork()`, `execve()`, `waitpid()`, `mmap()`, `brk()`, `ioctl()`, `poll()`, file I/O | `libc.a` / `iota.h` |
| **Sound / PC Speaker** | **Supported** | Sound Blaster 16 DMA PCM (OSS-style `/dev/dsp` + `/dev/mixer`, QEMU `-device sb16`) and PIT channel-2 speaker beeper | `/dev/dsp` / `/bin/beep` / `/bin/play` |

---

## 🌟 Key Features

### 🖥️ Desktop, Graphics & Gaming
- **LightDM Graphical Display Manager (`/bin/lightdm`):** Elegant, modern login screen and session manager. Provides quick user account selection (`root` and `iota`), session switching, password authentication, and power management (Reboot, Shut Down, Console).
- **XFCE 4 Desktop Environment (`/bin/xfce`):** Authentic, lightweight desktop experience featuring dual-panel architecture (Top Panel with Whisker menu, workspace switcher, task buttons, and live system tray; Bottom floating dock with app launchers), signature XFWM4 window shade/roll-up (`[^]`), Thunar file manager, and XFCE Terminal.
- **Ultra-Fast 90+ FPS Rendering Pipeline:** Precomputed integer trigonometric and square-root lookup tables (LUTs) combined with dirty-rectangle window and cursor blitting eliminate floating-point bottlenecks, achieving a steady 90–120 FPS in QEMU software CPU emulation.
- **High Refresh Rate Pacing (60Hz – 144Hz & Uncapped):** Dynamic frame pacing with instant toggle hotkey (<kbd>F11</kbd>) and real-time FPS benchmarking displayed directly in the system tray.
- **Wayland-Gears 3D Benchmark (`/bin/gears`):** Real-time 3D rotating gears demo featuring mathematical tooth meshing, directional surface shading, speed controls, and multiple themes. Runs standalone or inside an XFCE desktop window.
- **DOOM 3D Engine (`/bin/doom`):** Raycasted 3D first-person game engine featuring procedural textured stone and metal walls, classic Doomguy health/armor status bar, animated double-barrel shotgun, hitscan combat, and minimap.
- **Retro CHIP-8 Emulator 2.0 (`/bin/emu`):** Built-in retro virtual machine executing classic video games at 600 Hz on a phosphor green CRT monitor.
- **Hypervisor Mouse Calibration:** Differentiates the host machine's cursor from the virtual guest cursor with 4 high-visibility styles (Breeze VM Glow with `[VM]` badge, Precision Target Ring, Neon Amber, and Ghost Halo). Calibrates sensitivity (85% smoothed for virtual machines).

### 💾 Storage & Persistent Disks
- **ATA / IDE Hard Disk Controller (`/dev/hda`):** Direct PIO sector read and write support on Primary ATA bus (`0x1F0`–`0x1F7`) with hardware cache flush. Any file or raw data written to `/dev/hda` persists inside the QEMU hard disk image (`build/hda.img`) across system reboots.
- **In-Memory VFS & Initrd:** Writable filesystem mounted from a standard `ustar` ramdisk with permission checks, sticky bits (`/tmp` 1777), `/dev` device nodes, and home directories.

### 🧪 Diagnostics & Automated Verification
- **Automated Test Suite (`/bin/tests`):** Comprehensive 11-step verification suite testing Dynamic Heap, VFS I/O, Command Arguments, Environment, ANSI, ATA Disk persistence, Framebuffer geometry, Mouse driver, x87 FPU Math, PMM memory limits, and SB16 audio (`/dev/dsp` formats, mixer, poll).
- **Boot Diagnostics Console:** Pause during boot by pressing <kbd>DEL</kbd>, <kbd>ESC</kbd>, or <kbd>F2</kbd> to inspect CPUID features, memory maps, RTC time, and PIC interrupt masks.
- **Kernel Crash Screen:** Diagnostic Blue Screen of Death (BSOD) displaying CPU registers, stack dumps, and exception details on fatal panics.

### 🔊 Audio & Sound
- **Sound Blaster 16 PCM Audio (`/dev/dsp`, `/dev/audio`):** OSS-compatible playback (`SNDCTL_DSP_*` ioctls, 8-bit unsigned / 16-bit signed, mono/stereo, 5–45 kHz) driven by ISA DMA (ch1/ch5) and IRQ5 with a 32 KiB auto-init ring on QEMU's `-device sb16`. `/bin/play 440 2` renders a sine tone straight to the card.
- **FM Music (`/dev/opl`):** register pipe to the card's OPL3 chip (0x388/0x389). `doom` plays the classic DMX-MUS scoretracks (title `D_VICTOR`, per-level `D_E1M1` …) through it with a 9-voice allocator — authentic FM music, synthesized by the emulated chip itself.
- **Mixer & PC Speaker (`/dev/mixer`, `KIOCSOUND`):** Software master/PCM volume (`MIXER_READ`/`MIXER_WRITE`) with SB16 mixer registers programmed for real hardware; PIT channel-2 beeper reachable from any tty — `/bin/beep -f 880 -l 300` works even without a sound card. The terminal bell (`\a`) sounds 200 ms of tone and auto-mutes via the timer tick.

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
- [x] IDE/ATA Primary PIO disk controller driver (`/dev/hda`) with 512-byte persistent sector I/O.
- [ ] Read/write filesystem implementation (ext2 or FAT32) on `/dev/hda`.
- [ ] Partition table parsing (MBR / GPT).
- [ ] Standalone installer image (`IotaOSInstaller.iso`) for permanent disk installations.

### 🌐 Networking (v1.2)
- [ ] Network Interface Card (NIC) drivers (e.g. Realtek RTL8139 and Intel e1000).
- [ ] Kernel network packet buffer architecture (`sk_buff` style).
- [ ] Basic TCP/IP network stack (ARP, IPv4, ICMP ping, UDP, TCP).
- [ ] Socket syscalls (`sys_socket`, `sys_connect`, `sys_bind`, `sys_listen`, `sys_accept`).

### 🔊 Audio (v1.3)
- [x] Sound Blaster 16 DSP driver (probe, 0x41 raw-Hz rate, auto-init DMA output) on QEMU `-device sb16`.
- [x] ISA 8237 DMA channels 1 (8-bit) and 5 (16-bit) with 64 KiB-boundary-safe 32 KiB ring.
- [x] OSS-style `/dev/dsp` + `/dev/audio` playback device (`SNDCTL_DSP_*` ioctls, `poll`, blocking writes, SYNC drain).
- [x] `/dev/mixer` with `MIXER_READ`/`MIXER_WRITE` master/PCM volume (software gain + SB16 mixer registers).
- [x] PC speaker via `KIOCSOUND` on ttys, `/bin/beep` and `/bin/play` demo tools, audio entry in `/bin/tests` (11/11).
- [ ] Capture (`/dev/dsp` read path, mixer record levels).
- [x] Doom SFX + classic FM music: `i_iotasound.c` sound module (DMX lump cache, 8-channel u8 mixer into `/dev/dsp`) and DMX-MUS -> OPL player over `/dev/opl`.
- [x] OPL3 FM music via the `/dev/opl` register pipe (chip-side synthesis; `/dev/sequencer` not needed).

### ⚙️ Kernel Enhancements & Future Architecture
- [ ] Dynamic linking and shared object (`.so`) loader.
- [ ] User-space device driver framework experimentation.
- [ ] Ported text editor (nano or vim) and self-hosted C compiler.

---

## 📄 License

IotaOS is released under the [MIT License](LICENSE).  
Copyright &copy; 2026 grish-ka.