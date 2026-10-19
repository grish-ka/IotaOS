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
STRIP = i686-elf-strip
# Flags
CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-pic -I$(SRC_DIR)/kernel

# Kernel Sources
C_SOURCES = $(wildcard $(SRC_DIR)/kernel/*.c) $(wildcard $(SRC_DIR)/kernel/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/bootloader/*.s) $(wildcard $(SRC_DIR)/kernel/*.s) $(wildcard $(SRC_DIR)/kernel/*/*.s)

C_OBJECTS = $(C_SOURCES:.c=.o)
ASM_OBJECTS = $(ASM_SOURCES:.s=.o)
ALL_OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Programs: IotaOS demo apps and the coreutils, all installed into /bin
CORE_DIR = $(SRC_DIR)/coreutils
APP_SOURCES = $(wildcard $(APPS_DIR)/*.c)
CORE_SOURCES = $(wildcard $(CORE_DIR)/*.c)
BIN_DIR = $(BUILD_DIR)/bin

LIBC_DIR = src/IotaADK/libc
LIBC_SOURCES = $(wildcard $(LIBC_DIR)/src/*.c)
LIBC_OBJECTS = $(patsubst $(LIBC_DIR)/src/%.c, $(BUILD_DIR)/libc_%.o, $(LIBC_SOURCES))

APP_BINARIES = $(patsubst $(APPS_DIR)/%.c, $(BIN_DIR)/%, $(APP_SOURCES)) \
		$(patsubst $(CORE_DIR)/%.c, $(BIN_DIR)/%, $(CORE_SOURCES))

# Third-party sources (downloaded, not in git): Bash, zlib, mandoc
GNU_DIR = ./src/gnu
ZLIB_VER = 1.3.1
ZLIB_SRC = $(GNU_DIR)/zlib-$(ZLIB_VER)
ZLIB_TAR = $(GNU_DIR)/zlib-$(ZLIB_VER).tar.gz
ZLIB_OBJS = adler32 crc32 deflate infback inffast inflate inftrees trees zutil \
	compress uncompr gzclose gzlib gzread gzwrite
MANDOC_VER = 1.14.6
MANDOC_SRC = $(GNU_DIR)/mandoc-$(MANDOC_VER)
MANDOC_TAR = $(GNU_DIR)/mandoc-$(MANDOC_VER).tar.gz
HOST_MANDOC_DIR = $(BUILD_DIR)/host-mandoc
HOST_MAKEWHATIS = $(HOST_MANDOC_DIR)/makewhatis

# Root filesystem image: rootfs/ (config files) + programs, as a ustar initrd
ROOTFS_SRC = rootfs
ROOT = $(BUILD_DIR)/initrd_root
INITRD = $(BUILD_DIR)/iso/boot/initrd.img
# Extra names for programs, as NAME:TARGET hard links in /bin
BIN_LINKS = man:mandoc apropos:mandoc whatis:mandoc makewhatis:mandoc poweroff:reboot halt:reboot shutdown:reboot awk:gawk gunzip:gzip zcat:gzip sh:bash doomgeneric:doom raycaster:doomclassic

all: $(BUILD_DIR)/IotaOS $(BUILD_DIR)/libc.a $(BIN_DIR)/doom $(APP_BINARIES)

gnu-all: gnu-bash

# --- KERNEL BUILD ---
$(BUILD_DIR)/IotaOS: $(ALL_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) -T $(SRC_DIR)/linker.ld -o $(BUILD_DIR)/IotaOS -ffreestanding -O2 -nostdlib $(ALL_OBJECTS) -lgcc


# --- LIBC BUILD ---

$(BUILD_DIR)/libc.a: $(LIBC_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	@echo "Archiving libc.a..."
	rm -f $@
	$(AR) rcs $@ $^

$(BUILD_DIR)/libc_%.o: $(LIBC_DIR)/src/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) -c $< -o $@ $(CFLAGS) -I$(LIBC_DIR)/include -Isrc/IotaADK/include

# --- C APP BUILD ---

$(BUILD_DIR)/crt0.o: $(ADK_DIR)/lib/crt0.s
	@mkdir -p $(BUILD_DIR)
	$(AS) $< -o $@

APP_CFLAGS = $(CFLAGS) -I$(ADK_DIR)/include -I$(LIBC_DIR)/include -fno-asynchronous-unwind-tables
APP_LINK = $(LD) -m elf_i386 -T $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/crt0.o $(BUILD_DIR)/$*.o -L$(BUILD_DIR) -lc $(shell $(CC) -print-libgcc-file-name)

# --- DOOMGENERIC AUTHENTIC ID SOFTWARE DOOM ENGINE ---
DOOM_DIR = src/doomgeneric/doomgeneric
DOOM_SRCS = dummy.c am_map.c doomdef.c doomstat.c dstrings.c d_event.c d_items.c \
	d_iwad.c d_loop.c d_main.c d_mode.c d_net.c f_finale.c f_wipe.c \
	g_game.c hu_lib.c hu_stuff.c info.c i_cdmus.c i_endoom.c i_joystick.c \
	i_scale.c i_sound.c i_system.c i_timer.c memio.c m_argv.c m_bbox.c \
	m_cheat.c m_config.c m_controls.c m_fixed.c m_menu.c m_misc.c m_random.c \
	p_ceilng.c p_doors.c p_enemy.c p_floor.c p_inter.c p_lights.c p_map.c \
	p_maputl.c p_mobj.c p_plats.c p_pspr.c p_saveg.c p_setup.c p_sight.c \
	p_spec.c p_switch.c p_telept.c p_tick.c p_user.c r_bsp.c r_data.c \
	r_draw.c r_main.c r_plane.c r_segs.c r_sky.c r_things.c sha1.c \
	sounds.c statdump.c st_lib.c st_stuff.c s_sound.c tables.c v_video.c \
	wi_stuff.c w_checksum.c w_file.c w_main.c w_wad.c z_zone.c w_file_stdc.c \
	i_input.c i_video.c doomgeneric.c doomgeneric_iota.c i_iotasound.c
DOOM_OBJS = $(patsubst %.c, $(BUILD_DIR)/doomgeneric/%.o, $(DOOM_SRCS))

$(BUILD_DIR)/doomgeneric/%.o: $(DOOM_DIR)/%.c
	@mkdir -p $(BUILD_DIR)/doomgeneric
	$(CC) -c $< -o $@ $(APP_CFLAGS) -I$(DOOM_DIR) -DNORMALUNIX -DLINUX -D_DEFAULT_SOURCE -DFEATURE_SOUND -Wno-unused-parameter -Wno-unused-variable

$(BIN_DIR)/doom: $(DOOM_OBJS) $(BUILD_DIR)/crt0.o $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/libc.a
	@mkdir -p $(BIN_DIR)
	@echo "Linking authentic id Software DOOM engine: $@"
	$(LD) -m elf_i386 -T $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/crt0.o $(DOOM_OBJS) -L$(BUILD_DIR) -lc $(shell $(CC) -print-libgcc-file-name) -o $@
	$(STRIP) $@

$(BIN_DIR)/%: $(APPS_DIR)/%.c $(BUILD_DIR)/crt0.o $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/libc.a
	@mkdir -p $(BIN_DIR)
	@echo "Building C App using ADK: $<"
	$(CC) -c $< -o $(BUILD_DIR)/$*.o $(APP_CFLAGS)
	$(APP_LINK) -o $@

$(BIN_DIR)/%: $(CORE_DIR)/%.c $(CORE_DIR)/common.h $(BUILD_DIR)/crt0.o $(ADK_DIR)/lib/linker.ld $(BUILD_DIR)/libc.a
	@mkdir -p $(BIN_DIR)
	$(CC) -c $< -o $(BUILD_DIR)/$*.o $(APP_CFLAGS)
	$(APP_LINK) -o $@
	$(STRIP) $@

# --- ROOT FILESYSTEM & ISO ---
initrd: all bash-vfs ports $(BIN_DIR)/mandoc $(HOST_MAKEWHATIS)
	@echo "Assembling root filesystem..."
	rm -rf $(ROOT)
	mkdir -p $(ROOT)/bin $(ROOT)/dev $(ROOT)/etc $(ROOT)/home $(ROOT)/mnt $(ROOT)/root \
		$(ROOT)/tmp $(ROOT)/usr/bin $(ROOT)/usr/local/bin $(ROOT)/usr/share $(ROOT)/usr/share/man $(ROOT)/var/log $(ROOT)/var/mail
	cp -a $(ROOTFS_SRC)/. $(ROOT)/
	cp $(BIN_DIR)/* $(ROOT)/bin/
	@for link in $(BIN_LINKS); do \
		name=$${link%%:*}; target=$${link#*:}; \
		if [ -f $(ROOT)/bin/$$target ]; then ln -f $(ROOT)/bin/$$target $(ROOT)/bin/$$name; fi; \
	done
	ln -f $(ROOT)/bin/env $(ROOT)/usr/bin/env
	$(HOST_MAKEWHATIS) $(ROOT)/usr/share/man
	chmod 4755 $(ROOT)/bin/su $(ROOT)/bin/sudo
	chmod 700 $(ROOT)/root
	chmod 1777 $(ROOT)/tmp
	@mkdir -p $(dir $(INITRD))
	cd $(ROOT) && tar --format=ustar --owner=0 --group=0 --numeric-owner --exclude=./home/iota -cf $(abspath $(INITRD)) .
	cd $(ROOT) && tar --format=ustar --owner=1000 --group=1000 --numeric-owner -rf $(abspath $(INITRD)) ./home/iota
	@echo "initrd: $$(du -h $(INITRD) | cut -f1), $$(find $(ROOT) | wc -l) entries"

iso: $(BUILD_DIR)/IotaOS initrd
	@mkdir -p $(BUILD_DIR)/iso/boot/grub
	cp $(BUILD_DIR)/IotaOS $(BUILD_DIR)/iso/boot/IotaOS
	cp grub.cfg $(BUILD_DIR)/iso/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD_DIR)/IotaOS.iso $(BUILD_DIR)/iso

$(BUILD_DIR)/hda.img:
	@mkdir -p $(BUILD_DIR)
	@if [ ! -f $@ ]; then qemu-img create -f raw $@ 64M; fi


# Sound Blaster 16 (ISA: IRQ5, DMA1/5 on the pc machine). Disable with
# `make run QEMU_AUDIO=`; if this QEMU build lacks -audiodev sdl, use
# alsa/pa, or `none` for a silent device.
QEMU_AUDIO = -device sb16,audiodev=snd0 -audiodev sdl,id=snd0

run: iso $(BUILD_DIR)/hda.img
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -hda $(BUILD_DIR)/hda.img $(QEMU_AUDIO)

run-cursor: iso $(BUILD_DIR)/hda.img
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -hda $(BUILD_DIR)/hda.img -show-cursor $(QEMU_AUDIO)

debug: iso $(BUILD_DIR)/hda.img
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -hda $(BUILD_DIR)/hda.img -serial stdio $(QEMU_AUDIO)

# Start frozen, with the QEMU monitor on this terminal: type `c` to run,
# `stop` to pause again, `info registers` / `x/16x ADDR` to inspect, `quit` to exit
pause: iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -S -monitor stdio $(QEMU_AUDIO)

sync:
	@chmod +x tools/sync_tasks.sh
	@./tools/sync_tasks.sh
	@echo "Local Taskwarrior Status:"
	@task project:IotaOS next

clean:
	rm -rf $(ALL_OBJECTS) $(BUILD_DIR)

# --- GNU TOOLCHAIN PORTING ---
BASH_VER = 5.2.21
BASH_SRC = $(GNU_DIR)/bash-$(BASH_VER)
BASH_TAR = bash-$(BASH_VER).tar.gz

# This uses abspath so the configure script doesn't get lost when it changes directories
GNU_CFLAGS = -I$(abspath $(LIBC_DIR)/include) -I$(abspath $(ADK_DIR)/include)
GNU_LDFLAGS = -L$(abspath $(BUILD_DIR)) -lc

clean-gnu:
	@echo "Cleaning GNU build directories..."
	rm -rf $(BASH_SRC)

gnu-bash: $(BUILD_DIR)/libc.a $(BUILD_DIR)/crt0.o
	@mkdir -p $(GNU_DIR)
	@if [ ! -f $(GNU_DIR)/$(BASH_TAR) ]; then \
		echo "Downloading GNU Bash $(BASH_VER)..."; \
		wget -q -P $(GNU_DIR) https://ftp.gnu.org/gnu/bash/$(BASH_TAR); \
	fi
	@if [ ! -d $(BASH_SRC) ]; then \
		echo "Extracting Bash..."; \
		tar -xzf $(GNU_DIR)/$(BASH_TAR) -C $(GNU_DIR); \
	fi
	@echo "Configuring Bash for IotaOS cross-compilation..."
	cd $(BASH_SRC) && \
	CC="$(CC)" \
	CFLAGS="-O2 -DHAVE_TIMEVAL -DTERMIOS_TTY=1 $(GNU_CFLAGS) -ffreestanding" \
	LDFLAGS="-nostdlib -T $(abspath $(ADK_DIR)/lib/linker.ld) $(abspath $(BUILD_DIR)/crt0.o) $(GNU_LDFLAGS)" \
	LIBS="-L$(abspath $(BUILD_DIR)) -lc -lgcc" \
	CC_FOR_BUILD="gcc -std=gnu89" \
	./configure --build=x86_64-pc-linux-gnu --host=i686-elf --without-bash-malloc --disable-multibyte --disable-nls ac_cv_sizeof_char=1 ac_cv_sizeof_short=2 ac_cv_sizeof_int=4 ac_cv_sizeof_long=4 ac_cv_sizeof_char_p=4 ac_cv_sizeof_size_t=4 ac_cv_sizeof_long_long=8 ac_cv_type_off_t=yes ac_cv_type_mode_t=yes ac_cv_type_uid_t=yes ac_cv_type_gid_t=yes ac_cv_type_pid_t=yes ac_cv_type_ssize_t=yes ac_cv_type_time_t=yes ac_cv_type_u_int=yes ac_cv_type_u_long=yes ac_cv_type_clock_t=yes ac_cv_type_sigset_t=yes ac_cv_func_strerror=yes ac_cv_type_intmax_t=yes ac_cv_type_uintmax_t=yes ac_cv_sizeof_intmax_t=8 ac_cv_sizeof_uintmax_t=8 ac_cv_type_struct_timeval=yes ac_cv_func_isblank=yes ac_cv_func_vprintf=yes ac_cv_header_termios_h=yes ac_cv_func_tcgetattr=yes bash_cv_sys_posix_termios=yes ac_cv_header_sys_ioctl_h=yes bash_cv_struct_winsize_header=sys/ioctl.h ac_cv_header_dirent_h=yes
	@echo "Patching config.h and shtty.h to fix mksyntax and termios..."
	sed -i 's/#define clock_t long/\/\* patched clock_t \*\//g' $(BASH_SRC)/config.h
	sed -i 's/#define sigset_t int/\/\* patched sigset_t \*\//g' $(BASH_SRC)/config.h
	sed -i 's/#define sig_atomic_t int/\/\* patched sig_atomic_t \*\//g' $(BASH_SRC)/config.h
	sed -i 's/#define socklen_t unsigned int/\/\* patched socklen_t \*\//g' $(BASH_SRC)/config.h
	sed -i 's/<sgtty.h>/<termios.h>/g' $(BASH_SRC)/include/shtty.h
	sed -i 's/#define uintptr_t unsigned int/\/\* patched uintptr_t \*\//g' $(BASH_SRC)/config.h
	@echo "extern char *shell_input_line_property;" >> $(BASH_SRC)/config.h
	@echo "#undef TERMIOS_MISSING" >> $(BASH_SRC)/config.h
	@echo "#define _POSIX_VERSION 200809L" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_TERMIOS_H 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_TCGETATTR 1" >> $(BASH_SRC)/config.h
	@echo "#define TERMIOS_TTY_DRIVER 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_SYS_TIME_H 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_STRPBRK 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_GETHOSTNAME 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_POSIX_SIGNALS 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_PSELECT 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_GETCWD 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_WAITPID 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_SYS_WAIT_H 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_RENAME 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_DPRINTF 1" >> $(BASH_SRC)/config.h
	@echo "#define HAVE_DUP2 1" >> $(BASH_SRC)/config.h
	@echo "#define USEC_PER_SEC 1000000" >> $(BASH_SRC)/config.h
	@echo "#ifndef PARAMS" >> $(BASH_SRC)/config.h
	@echo "#define PARAMS(protos) protos" >> $(BASH_SRC)/config.h
	@echo "#endif" >> $(BASH_SRC)/config.h
	@echo "Building Bash..."
	$(MAKE) -C $(BASH_SRC)

# --- BASH -> VFS ---
# Installs the cross-compiled bash as /bin/bash (and /bin/sh). If it has not
# been built yet, asks whether to build it first. Answer non-interactively with
# WITH_BASH=y or WITH_BASH=n (no terminal and no WITH_BASH means skip).
BASH_BIN = $(BASH_SRC)/bash
BASH_VFS = $(BIN_DIR)/bash

bash-vfs: $(BUILD_DIR)/libc.a $(BUILD_DIR)/crt0.o
	@if [ ! -f $(BASH_BIN) ]; then \
		answer="$(WITH_GNU)"; \
		if [ -z "$$answer" ]; then answer="$(WITH_BASH)"; fi; \
		if [ -z "$$answer" ] && [ -t 0 ]; then \
			echo ""; \
			echo "GNU Bash is not compiled. Do you want to compile GNU?"; \
			echo "  1) Yes"; \
			echo "  2) Exit"; \
			printf "Enter choice [1/2]: "; \
			read choice; \
			case "$$choice" in \
				1|[yY]|[yY][eE][sS]) answer="y" ;; \
				*) echo "Build aborted by user."; exit 1 ;; \
			esac; \
		fi; \
		case "$$answer" in \
			[yY]|[yY][eE][sS]|1) \
				if [ -f $(BASH_SRC)/config.h ]; then \
					$(MAKE) -C $(BASH_SRC) || exit 1; \
				else \
					$(MAKE) gnu-bash || exit 1; \
				fi ;; \
			*) \
				echo "Build aborted by user."; \
				exit 1 ;; \
		esac; \
	fi; \
	mkdir -p $(BIN_DIR); \
	if [ ! -f $(BASH_VFS) ] || [ $(BASH_BIN) -nt $(BASH_VFS) ]; then \
		echo "Copying Bash to the VFS (stripped)..."; \
		$(STRIP) -o $(BASH_VFS) $(BASH_BIN); \
	fi

# --- OFFICIAL GNU / UTILITY PORTS ---
# less, more, sed, grep, awk, diff, gzip, tar, coreutils and findutils are the
# real packages, cross-compiled by tools/build-ports.sh into build/bin.
ports:
	@if [ ! -f $(BIN_DIR)/.ports ]; then \
		answer="$(WITH_GNU)"; \
		if [ -z "$$answer" ]; then answer="$(WITH_PORTS)"; fi; \
		if [ -z "$$answer" ] && [ -t 0 ]; then \
			echo ""; \
			echo "GNU ports are not compiled. Do you want to compile GNU?"; \
			echo "  1) Yes"; \
			echo "  2) Exit"; \
			printf "Enter choice [1/2]: "; \
			read choice; \
			case "$$choice" in \
				1|[yY]|[yY][eE][sS]) answer="y" ;; \
				*) echo "Build aborted by user."; exit 1 ;; \
			esac; \
		fi; \
		case "$$answer" in \
			[yY]|[yY][eE][sS]|1) \
				mkdir -p $(BIN_DIR); \
				PATH="$(PATH)" tools/build-ports.sh || exit 1; \
				touch $(BIN_DIR)/.ports ;; \
			*) \
				echo "Build aborted by user."; \
				exit 1 ;; \
		esac; \
	fi

clean-ports:
	rm -f $(BIN_DIR)/.ports

ports-rebuild: clean-ports ports

# --- MANUAL PAGES: zlib + mandoc ---
# mandoc provides man/apropos/whatis and reads the gzipped pages in
# rootfs/usr/share/man. A second, native mandoc builds their search database
# (mandoc.db) while the root filesystem is assembled.

$(ZLIB_TAR):
	@mkdir -p $(GNU_DIR)
	wget -q -O $@ https://zlib.net/fossils/zlib-$(ZLIB_VER).tar.gz

$(ZLIB_SRC)/zlib.h: $(ZLIB_TAR)
	tar -xzf $< -C $(GNU_DIR)
	touch $@

$(BUILD_DIR)/libz.a: $(ZLIB_SRC)/zlib.h
	@mkdir -p $(BUILD_DIR)/zlib
	for f in $(ZLIB_OBJS); do \
		$(CC) -c -O2 -ffreestanding -fno-pie -fno-pic -I$(LIBC_DIR)/include -I$(ADK_DIR)/include \
			$(ZLIB_SRC)/$$f.c -o $(BUILD_DIR)/zlib/$$f.o || exit 1; \
	done
	rm -f $@
	$(AR) rcs $@ $(BUILD_DIR)/zlib/*.o

$(MANDOC_TAR):
	@mkdir -p $(GNU_DIR)
	wget -q -O $@ https://mandoc.bsd.lv/snapshots/mandoc-$(MANDOC_VER).tar.gz

$(MANDOC_SRC)/configure: $(MANDOC_TAR)
	tar -xzf $< -C $(GNU_DIR)
	touch $@

$(BIN_DIR)/mandoc: $(MANDOC_SRC)/configure tools/mandoc.configure.local $(BUILD_DIR)/libz.a
	@mkdir -p $(BIN_DIR)
	sed 's|@ROOT@|$(abspath .)|g' tools/mandoc.configure.local > $(MANDOC_SRC)/configure.local
	cd $(MANDOC_SRC) && ./configure > configure.out 2>&1 && rm -f mandoc && $(MAKE) mandoc
	$(STRIP) -o $@ $(MANDOC_SRC)/mandoc

# Native build for the machine running make (needs the host's zlib headers)
$(HOST_MAKEWHATIS): $(MANDOC_SRC)/configure
	rm -rf $(HOST_MANDOC_DIR)
	mkdir -p $(HOST_MANDOC_DIR)
	cd $(MANDOC_SRC) && find . -maxdepth 1 -type f ! -name configure.local ! -name config.h \
		! -name Makefile.local ! -name '*.o' ! -name '*.a' ! -name mandoc -exec cp {} $(abspath $(HOST_MANDOC_DIR))/ \;
	cd $(HOST_MANDOC_DIR) && CC=gcc ./configure > configure.out 2>&1 && $(MAKE) mandoc
	ln -sf mandoc $@

.PHONY: pause ports all gnu-all gnu-bash clean clean-gnu run debug sync bash-vfs initrd iso clean-ports ports-rebuild