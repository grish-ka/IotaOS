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
BIN_LINKS = man:mandoc apropos:mandoc whatis:mandoc makewhatis:mandoc poweroff:reboot halt:reboot shutdown:reboot awk:gawk gunzip:gzip zcat:gzip sh:bash

all: $(BUILD_DIR)/IotaOS $(BUILD_DIR)/libc.a $(APP_BINARIES)

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
		$(ROOT)/tmp $(ROOT)/usr/bin $(ROOT)/usr/local/bin $(ROOT)/usr/share $(ROOT)/var/log $(ROOT)/var/mail
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

run: iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso

debug: iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -serial stdio

# Start frozen, with the QEMU monitor on this terminal: type `c` to run,
# `stop` to pause again, `info registers` / `x/16x ADDR` to inspect, `quit` to exit
pause: iso
	qemu-system-i386 -m 4096M -cdrom $(BUILD_DIR)/IotaOS.iso -S -monitor stdio

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
		answer="$(WITH_BASH)"; \
		if [ -z "$$answer" ] && [ -t 0 ]; then \
			printf "GNU Bash is not compiled. Compile it now and copy it to the VFS? [y/N] "; \
			read answer; \
		fi; \
		case "$$answer" in \
			[yY]|[yY][eE][sS]) \
				if [ -f $(BASH_SRC)/config.h ]; then \
					$(MAKE) -C $(BASH_SRC) || exit 1; \
				else \
					$(MAKE) gnu-bash || exit 1; \
				fi ;; \
			*) \
				echo "Skipping Bash; the VFS will not contain /bin/bash."; \
				rm -f $(BASH_VFS); \
				exit 0 ;; \
		esac; \
	elif [ $(BUILD_DIR)/libc.a -nt $(BASH_BIN) ]; then \
		echo "libc changed: relinking Bash..."; \
		rm -f $(BASH_BIN); \
		$(MAKE) -C $(BASH_SRC) || exit 1; \
	fi; \
	mkdir -p $(BIN_DIR); \
	if [ ! -f $(BASH_VFS) ] || [ $(BASH_BIN) -nt $(BASH_VFS) ]; then \
		echo "Copying Bash to the VFS (stripped)..."; \
		$(STRIP) -o $(BASH_VFS) $(BASH_BIN); \
	fi

# --- OFFICIAL GNU / UTILITY PORTS ---
# less, more, sed, grep, awk, diff, gzip, tar, coreutils and findutils are the
# real packages, cross-compiled by tools/build-ports.sh into build/bin.
# The stamp is renewed (programs relinked) whenever libc or the tooling changes.
$(BIN_DIR)/.ports: $(BUILD_DIR)/libc.a $(BUILD_DIR)/crt0.o tools/build-ports.sh tools/build-port.sh \
		tools/iota-cc tools/iota-config.site
	@mkdir -p $(BIN_DIR)
	PATH="$(PATH)" tools/build-ports.sh
	touch $@

ports: $(BIN_DIR)/.ports

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

$(BIN_DIR)/mandoc: $(MANDOC_SRC)/configure tools/mandoc.configure.local $(BUILD_DIR)/libz.a $(BUILD_DIR)/libc.a $(BUILD_DIR)/crt0.o
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

.PHONY: pause ports all gnu-all gnu-bash clean clean-gnu run debug sync bash-vfs initrd iso