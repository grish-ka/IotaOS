#!/usr/bin/env python3
#
# IotaOS - flash.py
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#
# Interactive TUI for safely flashing IotaOS ISO to USB drives with
# system disk protection, interactive selection, progress bar, and verification.
#

import os
import sys
import json
import time
import tty
import termios
import hashlib
import select
import subprocess
from pathlib import Path

# --- ANSI Colors & Styles ---
RESET = "\033[0m"
BOLD = "\033[1m"
DIM = "\033[2m"
RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
BLUE = "\033[34m"
MAGENTA = "\033[35m"
CYAN = "\033[36m"
WHITE = "\033[37m"

BG_RED = "\033[41m"
BG_BLUE = "\033[44m"
BG_DARK = "\033[48;5;236m"
CLEAR_SCREEN = "\033[2J\033[H"
HIDE_CURSOR = "\033[?25l"
SHOW_CURSOR = "\033[?25h"

SYSTEM_MOUNTS = {"/", "/home", "/etc", "/usr", "/var", "/boot", "[SWAP]"}

def format_size(bytes_val):
    if bytes_val is None:
        return "Unknown"
    for unit in ["B", "KiB", "MiB", "GiB", "TiB"]:
        if bytes_val < 1024.0 or unit == "TiB":
            return f"{bytes_val:.1f} {unit}"
        bytes_val /= 1024.0
    return f"{bytes_val:.1f} TiB"

def get_key():
    """Read a single keypress or ANSI escape sequence from stdin."""
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        ch1 = sys.stdin.read(1)
        if ch1 == '\x1b':
            # Check if more characters follow (escape sequence)
            r, _, _ = select.select([sys.stdin], [], [], 0.05)
            if r:
                ch2 = sys.stdin.read(1)
                if ch2 == '[':
                    ch3 = sys.stdin.read(1)
                    if ch3 == 'A': return 'UP'
                    if ch3 == 'B': return 'DOWN'
                    if ch3 == 'C': return 'RIGHT'
                    if ch3 == 'D': return 'LEFT'
                    return f'ESC[{ch3}'
                return f'ESC{ch2}'
            return 'ESC'
        if ch1 in ('\r', '\n'): return 'ENTER'
        if ch1 == '\x03': return 'CTRL_C'
        if ch1 == '\x04': return 'CTRL_D'
        if ch1 == '\x7f' or ch1 == '\x08': return 'BACKSPACE'
        return ch1
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)

def scan_drives():
    """Query lsblk for all disks and partitions with JSON output."""
    try:
        cmd = ["lsblk", "-J", "-b", "-o", "NAME,PATH,SIZE,RO,TYPE,MOUNTPOINTS,MODEL,TRAN,RM,HOTPLUG"]
        res = subprocess.run(cmd, capture_output=True, text=True, check=True)
        data = json.loads(res.stdout)
    except Exception as e:
        print(f"{RED}Error running lsblk: {e}{RESET}")
        return []

    drives = []
    for dev in data.get("blockdevices", []):
        if dev.get("type") != "disk":
            continue

        # Check for system mountpoints across the disk and all its partitions
        has_system_mount = False
        all_mounts = []
        partitions = []

        def check_mounts(node):
            nonlocal has_system_mount
            for mp in node.get("mountpoints", []) or []:
                if mp:
                    all_mounts.append(mp)
                    for sys_m in SYSTEM_MOUNTS:
                        if mp == sys_m or mp.startswith(sys_m + "/"):
                            has_system_mount = True
            for child in node.get("children", []) or []:
                partitions.append({
                    "name": child.get("name"),
                    "path": child.get("path"),
                    "size": child.get("size"),
                    "mounts": child.get("mountpoints") or []
                })
                check_mounts(child)

        check_mounts(dev)

        model = (dev.get("model") or "").strip()
        tran = (dev.get("tran") or "").strip().lower()
        rm = bool(dev.get("rm"))
        ro = bool(dev.get("ro"))

        # Failsafe categorization
        is_usb = (tran == "usb") or (rm and "virtual" not in model.lower())
        is_protected = has_system_mount or ro or ("virtual disk" in model.lower() and not is_usb)

        status_text = ""
        if has_system_mount:
            status_text = "PROTECTED (System Mounted)"
        elif ro:
            status_text = "PROTECTED (Read-Only)"
        elif "virtual disk" in model.lower() and not is_usb:
            status_text = "PROTECTED (WSL Virtual Disk)"
        elif is_usb:
            status_text = "READY (USB Flash Drive)"
        else:
            status_text = "READY (Fixed Disk)"

        drives.append({
            "name": dev.get("name"),
            "path": dev.get("path"),
            "size": dev.get("size"),
            "model": model or "Generic Device",
            "tran": tran or ("usb" if rm else "internal"),
            "rm": rm,
            "ro": ro,
            "is_usb": is_usb,
            "is_protected": is_protected,
            "status_text": status_text,
            "mounts": all_mounts,
            "partitions": partitions
        })

    return drives

def ensure_iso(repo_root):
    """Ensure build/IotaOS.iso is built and return Path."""
    iso_path = repo_root / "build" / "IotaOS.iso"
    if not iso_path.exists():
        print(f"{YELLOW}IotaOS.iso not found. Building it now...{RESET}")
        env = os.environ.copy()
        if "/home/grish/opt/cross/bin" not in env.get("PATH", ""):
            env["PATH"] = f"/home/grish/opt/cross/bin:{env.get('PATH', '')}"
        res = subprocess.run(["make", "-C", str(repo_root), "iso"], env=env)
        if res.returncode != 0:
            print(f"{RED}Failed to build IotaOS.iso! Check build logs.{RESET}")
            sys.exit(1)
    return iso_path

def flash_drive(iso_path, target_drive):
    """Flash ISO to target drive with progress bar, sync, and SHA-256 verification."""
    dev_path = target_drive["path"]
    iso_size = iso_path.stat().st_size
    print(f"\n{CYAN}Unmounting any active partitions on {dev_path}...{RESET}")
    for p in target_drive["partitions"]:
        if p["mounts"]:
            subprocess.run(["umount", "-f", p["path"]], capture_output=True)

    print(f"{CYAN}Opening {iso_path.name} ({format_size(iso_size)}) for streaming...{RESET}")
    sha256_in = hashlib.sha256()

    chunk_size = 1024 * 1024  # 1 MiB chunks
    written = 0
    start_time = time.time()

    print(f"\n{BOLD}{GREEN}Writing to {dev_path}...{RESET}")
    print(HIDE_CURSOR, end="")

    try:
        with open(iso_path, "rb") as src, open(dev_path, "wb") as dst:
            while True:
                buf = src.read(chunk_size)
                if not buf:
                    break
                sha256_in.update(buf)
                dst.write(buf)
                written += len(buf)

                elapsed = time.time() - start_time
                speed = written / (elapsed if elapsed > 0 else 0.001)
                pct = (written / iso_size) * 100.0

                bar_len = 30
                filled = int(bar_len * (written / iso_size))
                bar = "█" * filled + "░" * (bar_len - filled)

                speed_str = f"{format_size(speed)}/s"
                eta = (iso_size - written) / (speed if speed > 0 else 1)
                eta_str = f"ETA: {eta:.1f}s" if written < iso_size else "Done!   "

                sys.stdout.write(
                    f"\r  [{CYAN}{bar}{RESET}] {BOLD}{pct:5.1f}%{RESET} "
                    f"({format_size(written)} / {format_size(iso_size)}) "
                    f"{YELLOW}{speed_str:>11}{RESET} | {eta_str}"
                )
                sys.stdout.flush()

            # Ensure disk controller commits to physical flash memory
            dst.flush()
            os.fsync(dst.fileno())

        print(f"\n{CYAN}Flushing system filesystem buffers (sync)...{RESET}")
        subprocess.run(["sync"])

    finally:
        print(SHOW_CURSOR, end="")

    iso_hash = sha256_in.hexdigest()
    print(f"{GREEN}Write finished in {time.time() - start_time:.2f}s!{RESET}")
    print(f"ISO SHA-256: {DIM}{iso_hash}{RESET}")

    # --- Verification Pass ---
    print(f"\n{CYAN}Verifying written data on {dev_path}...{RESET}")
    sha256_out = hashlib.sha256()
    read_bytes = 0
    v_start = time.time()

    try:
        print(HIDE_CURSOR, end="")
        with open(dev_path, "rb") as dst:
            while read_bytes < iso_size:
                to_read = min(chunk_size, iso_size - read_bytes)
                buf = dst.read(to_read)
                if not buf:
                    break
                sha256_out.update(buf)
                read_bytes += len(buf)

                pct = (read_bytes / iso_size) * 100.0
                bar_len = 30
                filled = int(bar_len * (read_bytes / iso_size))
                bar = "█" * filled + "░" * (bar_len - filled)

                sys.stdout.write(f"\r  [{GREEN}{bar}{RESET}] {BOLD}{pct:5.1f}% Verified{RESET}")
                sys.stdout.flush()
    finally:
        print(SHOW_CURSOR, end="")

    device_hash = sha256_out.hexdigest()
    print(f"\nDevice SHA-256: {DIM}{device_hash}{RESET}")

    if device_hash == iso_hash:
        print(f"\n{BOLD}{GREEN}============================================================{RESET}")
        print(f"{BOLD}{GREEN}  🎉 VERIFICATION PASSED! FLASHING COMPLETED SUCCESSFULLY!  {RESET}")
        print(f"{BOLD}{GREEN}============================================================{RESET}")
        print(f"You can now unplug {dev_path} and boot it on bare-metal hardware!\n")
    else:
        print(f"\n{BOLD}{RED}============================================================{RESET}")
        print(f"{BOLD}{RED}  ❌ CHECKSUM MISMATCH! THE FLASH DRIVE MAY HAVE ERRORS!   {RESET}")
        print(f"{BOLD}{RED}============================================================{RESET}\n")

def main():
    # Require root for block device raw access
    if os.geteuid() != 0:
        try:
            print(f"{YELLOW}Elevating with sudo to access disk block devices...{RESET}")
            os.execvp("sudo", ["sudo", "-E", sys.executable] + sys.argv)
        except Exception as e:
            print(f"{RED}Failed to elevate: {e}{RESET}")
            sys.exit(1)

    repo_root = Path(__file__).resolve().parent.parent
    iso_path = ensure_iso(repo_root)

    selected_idx = 0

    while True:
        drives = scan_drives()
        if not drives:
            print(f"{RED}No block devices found via lsblk!{RESET}")
            sys.exit(1)

        # Ensure index in bounds
        if selected_idx >= len(drives):
            selected_idx = len(drives) - 1

        # Clear and render TUI
        sys.stdout.write(CLEAR_SCREEN)
        print(f"{BOLD}{CYAN}╔════════════════════════════════════════════════════════════════════════════════╗{RESET}")
        print(f"{BOLD}{CYAN}║                    IotaOS Bare-Metal USB Flashing Tool                         ║{RESET}")
        print(f"{BOLD}{CYAN}╚════════════════════════════════════════════════════════════════════════════════╝{RESET}")
        
        iso_mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(iso_path.stat().st_mtime))
        print(f" Target Image: {BOLD}{WHITE}{iso_path.name}{RESET} ({format_size(iso_path.stat().st_size)}) | Built: {DIM}{iso_mtime}{RESET}\n")

        print(f"{BOLD}{'#':<4} {'Device':<11} {'Model':<20} {'Size':<11} {'Type':<8} {'Status'}{RESET}")
        print("─" * 80)

        for i, d in enumerate(drives):
            prefix = "► " if i == selected_idx else "  "
            num_str = f"[{i + 1}]"
            dev_str = d["path"]
            model_str = (d["model"][:18] + "..") if len(d["model"]) > 18 else d["model"]
            size_str = format_size(d["size"])
            type_str = "USB" if d["is_usb"] else d["tran"]

            if d["is_protected"]:
                status_color = RED
                status_badge = f"🔒 {d['status_text']}"
            elif d["is_usb"]:
                status_color = GREEN
                status_badge = f"⭐ {d['status_text']}"
            else:
                status_color = YELLOW
                status_badge = d['status_text']

            if i == selected_idx:
                line = f"{BOLD}{CYAN}{prefix}{num_str:<4} {dev_str:<11} {model_str:<20} {size_str:<11} {type_str:<8} {status_color}{status_badge}{RESET}"
            else:
                line = f"{prefix}{DIM}{num_str:<4}{RESET} {dev_str:<11} {model_str:<20} {size_str:<11} {type_str:<8} {status_color}{status_badge}{RESET}"

            print(line)

        print("─" * 80)
        print(f"{BOLD}Controls:{RESET} [↑/↓ or j/k] Navigate  [1-{len(drives)}] Quick Jump  [r] Rescan  [Enter] Select  [q] Quit\n")

        # Read user interaction
        key = get_key()

        if key in ('q', 'ESC', 'CTRL_C', 'CTRL_D'):
            print(f"\n{YELLOW}Aborted by user. No drives were modified.{RESET}")
            sys.exit(0)
        elif key in ('UP', 'k'):
            selected_idx = (selected_idx - 1) % len(drives)
        elif key in ('DOWN', 'j'):
            selected_idx = (selected_idx + 1) % len(drives)
        elif key == 'r':
            continue
        elif key.isdigit():
            idx = int(key) - 1
            if 0 <= idx < len(drives):
                selected_idx = idx
        elif key == 'ENTER':
            target = drives[selected_idx]

            # --- FAILSAFE CHECK ---
            if target["is_protected"]:
                print(f"\n{BOLD}{BG_RED}{WHITE} [ERROR: PROTECTED DRIVE] {RESET}")
                print(f"{RED}Cannot select {target['path']}! It is marked as {target['status_text']}.{RESET}")
                print(f"{RED}Writing to this drive would destroy your WSL/Host system.{RESET}")
                print(f"{YELLOW}Press any key to continue...{RESET}")
                get_key()
                continue

            # --- CONFIRMATION SCREEN ---
            sys.stdout.write(CLEAR_SCREEN)
            print(f"{BOLD}{RED}╔════════════════════════════════════════════════════════════════════════════════╗{RESET}")
            print(f"{BOLD}{RED}║                        ⚠️   CONFIRM TARGET DRIVE                             ║{RESET}")
            print(f"{BOLD}{RED}╚════════════════════════════════════════════════════════════════════════════════╝{RESET}\n")

            print(f"  Target Device:  {BOLD}{WHITE}{target['path']}{RESET}")
            print(f"  Model / Vendor: {target['model']}")
            print(f"  Total Capacity: {format_size(target['size'])} ({target['size']:,} bytes)")
            print(f"  Interface:      {target['tran'].upper()} {'(Removable)' if target['rm'] else ''}")

            if target["partitions"]:
                print(f"\n  {YELLOW}Existing Partitions to be ERASED:{RESET}")
                for p in target["partitions"]:
                    m_str = f" (mounted on {', '.join(p['mounts'])})" if p['mounts'] else ""
                    print(f"    • {p['path']:<15} {format_size(p['size'])}{m_str}")
            else:
                print(f"\n  {DIM}No existing partitions found on disk.{RESET}")

            print(f"\n{BOLD}{BG_RED}{WHITE} CRITICAL WARNING: ALL DATA ON {target['path']} WILL BE DESTROYED! {RESET}\n")
            print(f"To prevent accidental data loss, please confirm:")
            confirm_phrase = target['name']  # e.g. "sde"
            user_input = input(f"Type '{BOLD}{confirm_phrase}{RESET}' or '{BOLD}YES{RESET}' to confirm flashing: ").strip()

            if user_input.lower() in (confirm_phrase.lower(), "yes"):
                flash_drive(iso_path, target)
                sys.exit(0)
            else:
                print(f"\n{YELLOW}Confirmation mismatch ('{user_input}'). Flashing aborted.{RESET}")
                print(f"{YELLOW}Press any key to return to menu...{RESET}")
                get_key()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print(f"\n{YELLOW}Aborted.{RESET}")
        sys.exit(0)
