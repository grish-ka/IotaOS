#!/bin/bash
# IotaOS - proof-run.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Boots QEMU exactly like `make run` and saves the framebuffer QEMU
# actually received to /tmp/proof.ppm every 2 seconds while it runs.
# After quitting the QEMU window, /tmp/proof.ppm shows the LAST frame
# from the black-screen moment - proof of whether pixels arrived.

set -u
ISO=build/IotaOS.iso
HDA=build/hda.img
SOCK=/tmp/iota-qmp.sock

rm -f "$SOCK" /tmp/proof.ppm

qemu-system-i386 -m 4096M -cdrom "$ISO" -hda "$HDA" \
    -display gtk -monitor unix:"$SOCK",server,nowait \
    -device sb16,audiodev=snd0 -audiodev sdl,id=snd0 &
QPID=$!

# Wait for the monitor socket to appear
for _ in $(seq 1 50); do [ -S "$SOCK" ] && break; sleep 0.2; done

# Screendump loop: keeps overwriting /tmp/proof.ppm with the current frame
(
python3 - "$SOCK" <<'EOF'
import socket, time, sys, os
sock = sys.argv[1]
while True:
    try:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(2)
        s.connect(sock)
        s.recv(4096)
        s.sendall(b"screendump /tmp/proof.ppm\n")
        time.sleep(0.3)
        s.close()
    except Exception:
        break
    time.sleep(1.7)
EOF
) &
CPID=$!

wait $QPID
kill $CPID 2>/dev/null
[ -f /tmp/proof.ppm ] && echo "Frame captured while the window was up: /tmp/proof.ppm"
