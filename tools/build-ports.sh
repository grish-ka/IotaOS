#!/bin/sh
#
# IotaOS - build-ports.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Build the official GNU/utility packages for IotaOS into build/bin.
# Needs the cross-compiler on PATH, plus build/libc.a and build/crt0.o.
# Downloads, configures and builds each package once; re-run to relink the
# programs against a changed libc (Make does this when libc.a changes).

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
PORT="$ROOT/tools/build-port.sh"
SYSROOT="$ROOT/build/sysroot"
GNU="https://ftp.gnu.org/gnu"

mkdir -p "$ROOT/build/bin" "$SYSROOT/include" "$SYSROOT/lib"

# --- ncurses: the terminal library less, more and nano-like programs need ---
build_ncurses() {
    V=6.5
    SRC="$ROOT/src/gnu/ncurses-$V"
    [ -f "$ROOT/src/gnu/ncurses-$V.tar.gz" ] || wget -q -O "$ROOT/src/gnu/ncurses-$V.tar.gz" "$GNU/ncurses/ncurses-$V.tar.gz"
    if [ ! -f "$SYSROOT/lib/libncurses.a" ] || [ "$ROOT/build/libc.a" -nt "$SYSROOT/lib/libncurses.a" ] || \
       [ -n "$(find "$ROOT/src/IotaADK/libc/include" -newer "$SRC/.iota-configured" -type f 2>/dev/null | head -1)" ]; then
        rm -f "$SRC/.iota-configured"
        echo "Building ncurses $V..."
        if [ ! -f "$SRC/.iota-configured" ]; then
            rm -rf "$SRC"
            tar -xzf "$ROOT/src/gnu/ncurses-$V.tar.gz" -C "$ROOT/src/gnu"
            (cd "$SRC" && CC="$ROOT/tools/iota-cc" BUILD_CC=gcc CFLAGS="-O2" ./configure \
                --host=i686-linux-gnu --build="$(gcc -dumpmachine)" --prefix=/usr \
                --without-cxx --without-cxx-binding --without-ada --without-progs --without-tests \
                --without-manpages --without-shared --without-debug --disable-widec --disable-database \
                --with-fallbacks=linux --disable-home-terminfo --with-default-terminfo-dir=/usr/share/terminfo \
                --disable-stripping --with-termlib=no) > "$SRC/iota-configure.log" 2>&1
            touch "$SRC/.iota-configured"
        fi
        (cd "$SRC" && make -j"$(nproc)" libs) > "$SRC/iota-build.log" 2>&1
        cp "$SRC/lib/libncurses.a" "$SYSROOT/lib/"
        for l in curses tinfo termcap; do ln -sf libncurses.a "$SYSROOT/lib/lib$l.a"; done
        ln -sf libc.a "$SYSROOT/lib/libm.a"
        mkdir -p "$SYSROOT/include/ncurses"
        for h in curses.h term.h termcap.h unctrl.h ncurses_dll.h term_entry.h tic.h; do
            cp "$SRC/include/$h" "$SYSROOT/include/"
            cp "$SRC/include/$h" "$SYSROOT/include/ncurses/"
        done
        ln -sf curses.h "$SYSROOT/include/ncurses.h"
        ln -sf curses.h "$SYSROOT/include/ncurses/ncurses.h"
    fi
}

build_ncurses

# --- pagers ---
"$PORT" less 668 https://www.greenwoodsoftware.com/less/less-668.tar.gz "" less

# util-linux: only `more`. Its configure insists on Linux headers and on a wide
# ncurses found through the build machine's tools, so those are answered here.
NCURSESW6_CONFIG=no NCURSES6_CONFIG=no NCURSESW5_CONFIG=no NCURSES5_CONFIG=no PKG_CONFIG=/bin/false \
ac_cv_header_linux_version_h=yes PORT_MAKE_TARGETS=more \
"$PORT" util-linux 2.40.2 https://www.kernel.org/pub/linux/utils/util-linux/v2.40/util-linux-2.40.2.tar.xz \
    "--disable-all-programs --enable-more --disable-static-programs --without-python --without-systemd --without-udev --without-libmagic --without-selinux --without-audit --without-cryptsetup --disable-asciidoc --disable-poman --without-btrfs --without-econf --disable-year2038 --disable-largefile --with-ncurses --disable-widechar" more

# --- text tools ---
"$PORT" sed 4.9 $GNU/sed/sed-4.9.tar.xz "--disable-largefile --disable-year2038 --without-selinux --disable-acl" sed/sed
"$PORT" grep 3.11 $GNU/grep/grep-3.11.tar.xz "--disable-largefile --disable-year2038 --disable-perl-regexp" src/grep
"$PORT" gawk 5.3.0 $GNU/gawk/gawk-5.3.0.tar.xz "--disable-largefile --disable-year2038 --disable-mpfr --disable-pma --disable-extensions --without-readline" gawk
"$PORT" diffutils 3.10 $GNU/diffutils/diffutils-3.10.tar.xz "--disable-largefile --disable-year2038" src/diff src/cmp src/diff3 src/sdiff

# --- archives ---
"$PORT" gzip 1.13 $GNU/gzip/gzip-1.13.tar.xz "--disable-largefile --disable-year2038" gzip
"$PORT" tar 1.35 $GNU/tar/tar-1.35.tar.xz "--disable-largefile --disable-year2038 --without-selinux --without-posix-acls --without-xattrs" src/tar

# --- GNU coreutils and findutils ---
COREUTILS="[ b2sum base32 base64 basename basenc cat chgrp chmod chown cksum comm cp csplit cut date dd df dir dircolors dirname du echo env expand expr factor false fmt fold groups head id join link ln logname ls md5sum mkdir mkfifo mknod mktemp mv nice nl nohup nproc numfmt od paste pathchk pr printenv printf ptx pwd readlink realpath rm rmdir seq sha1sum sha224sum sha256sum sha384sum sha512sum shred shuf sleep sort split stat stty sum sync tac tail tee test timeout touch tr true truncate tsort tty uname unexpand uniq unlink vdir wc whoami yes"
# kill and uptime are left out: the versions in src/coreutils (kill with
# signal names, uptime with real figures) are the ones installed
args=""
for p in $COREUTILS; do args="$args src/$p"; done
args="$args src/ginstall=install"
FORCE_UNSAFE_CONFIGURE=1 PORT_MAKE_FLAGS=-k \
"$PORT" coreutils 9.5 $GNU/coreutils/coreutils-9.5.tar.xz \
    "--disable-largefile --disable-year2038 --without-selinux --disable-libcap --without-gmp --disable-acl --disable-xattr --enable-no-install-program=stdbuf --disable-single-binary" $args

PORT_MAKE_FLAGS=-k "$PORT" findutils 4.10.0 $GNU/findutils/findutils-4.10.0.tar.xz \
    "--disable-largefile --disable-year2038 --without-selinux --disable-leaf-optimisation" find/find xargs/xargs

echo "Ports built: $(ls "$ROOT/build/bin" | wc -l) programs in build/bin"
