#!/bin/sh
#
# IotaOS - build-port.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Download, configure, build and install one ported ./configure package.
#
#   build-port.sh NAME VERSION URL "CONFIGURE ARGS" PROGRAM[=INSTALLED_NAME]...
#
# Programs are paths inside the source tree; they are stripped into build/bin.
# Optional environment:
#   PORT_HOST          configure --host (default i686-linux-gnu, so packages
#                      take their Linux code paths)
#   PORT_MAKE_TARGETS  what to build (default: everything)
# Any other variables in the environment reach configure (cache values etc.).

set -e
ROOT=$(cd "$(dirname "$0")/.." && pwd)
NAME=$1 VERSION=$2 URL=$3 ARGS=$4
shift 4

SRC_DIR="$ROOT/src/gnu"
SRC="$SRC_DIR/$NAME-$VERSION"
TARBALL="$SRC_DIR/$(basename "$URL")"
BIN_DIR="$ROOT/build/bin"
HOST=${PORT_HOST:-i686-linux-gnu}
JOBS=$(nproc 2>/dev/null || echo 4)

mkdir -p "$SRC_DIR" "$BIN_DIR"
if [ ! -f "$TARBALL" ]; then
    echo "Downloading $NAME $VERSION..."
    wget -q -O "$TARBALL" "$URL"
fi

# configure's answers depend on what libc provides: redo them when libc's
# headers or the site file are newer than the last configure run
if [ -f "$SRC/.iota-configured" ]; then
    if [ "$ROOT/tools/iota-config.site" -nt "$SRC/.iota-configured" ] || \
       [ -n "$(find "$ROOT/src/IotaADK/libc/include" -newer "$SRC/.iota-configured" -type f 2>/dev/null | head -1)" ]; then
        rm -f "$SRC/.iota-configured"
    fi
fi

if [ ! -f "$SRC/.iota-configured" ]; then
    echo "Configuring $NAME $VERSION for IotaOS..."
    rm -rf "$SRC"
    tar -xf "$TARBALL" -C "$SRC_DIR"
    if ! (cd "$SRC" && CONFIG_SITE="$ROOT/tools/iota-config.site" CC="$ROOT/tools/iota-cc" \
            AR=i686-elf-ar RANLIB=i686-elf-ranlib CFLAGS="${PORT_CFLAGS:--O2}" \
            ./configure --host="$HOST" --build="$(gcc -dumpmachine)" --prefix=/usr \
            --disable-nls $ARGS) > "$SRC/iota-configure.log" 2>&1; then
        tail -20 "$SRC/iota-configure.log"
        echo "configure failed for $NAME; see $SRC/iota-configure.log"
        exit 1
    fi
    touch "$SRC/.iota-configured"
fi

# Relink the programs so they pick up the current libc
for prog in "$@"; do rm -f "$SRC/${prog%%=*}"; done

echo "Building $NAME $VERSION..."
# PORT_MAKE_FLAGS=-k keeps going past programs that fail, so one broken tool
# does not stop the rest; only the programs asked for must end up built
(cd "$SRC" && make -j"$JOBS" ${PORT_MAKE_FLAGS} ${PORT_MAKE_TARGETS}) > "$SRC/iota-build.log" 2>&1 || build_failed=1

missing=0
for prog in "$@"; do
    src=${prog%%=*}
    dest=${prog#*=}
    [ "$dest" = "$prog" ] && dest=$(basename "$src")
    if [ -f "$SRC/$src" ]; then
        i686-elf-strip -o "$BIN_DIR/$dest" "$SRC/$src"
    else
        echo "not built: $src"
        missing=1
    fi
done

if [ "${build_failed:-0}" = 1 ] && [ -z "${PORT_MAKE_FLAGS}" ]; then
    grep -E "error:|undefined reference" "$SRC/iota-build.log" | sort -u | head -20
    echo "build failed for $NAME; see $SRC/iota-build.log"
    exit 1
fi
if [ "$missing" = 1 ]; then
    grep -E "error:|undefined reference" "$SRC/iota-build.log" | sort -u | head -20
    echo "some programs of $NAME were not built; see $SRC/iota-build.log"
    exit 1
fi
