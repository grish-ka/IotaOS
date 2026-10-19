#!/usr/bin/env bash

#
# IotaOS - flash-usb.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

# IotaOS - tools/flash-usb.sh
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Launcher for the interactive Python TUI flasher (tools/flash.py).
#

DIR="$(cd "$(dirname "$0")" && pwd)"
exec python3 "$DIR/flash.py" "$@"
