#!/usr/bin/env python3

#
# IotaOS - license.py
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

import os

# Configuration
DIRECTORIES = ["src", "tools"]
EXPLICIT_FILES = ["Makefile", "grub.cfg"]
HOLDER = "grish-ka"
YEAR = "2026"

def get_style(filename):
    """Defines comment markers based on file extension."""
    if filename.endswith(('.c', '.h', '.ld', '.s')):
        return "/* ", " *", " */"
    if filename == "Makefile" or filename.endswith(('.py', '.sh', '.cfg')):
        return "# ", "# ", "#"
    return None, None, None

def generate_target_header(filename, start, mid, end):
    """Creates the exact list of lines we want to see."""
    m = mid.strip() + " " # Force exactly one space after the comment char
    return [
        f"{start.strip()}\n",
        f"{m}IotaOS - {filename}\n",
        f"{m}Copyright (c) {YEAR} {HOLDER}\n",
        f"{m}Licensed under the MIT License.\n",
        f"{end.strip()}\n"
    ]

def main():
    print(f"🚀 IotaOS License Guardian - Verifying 2026 Headers...")
    
    files = []
    for d in DIRECTORIES:
        if os.path.exists(d):
            for root, _, fs in os.walk(d):
                for f in fs: files.append(os.path.join(root, f))
    files.extend([f for f in EXPLICIT_FILES if os.path.exists(f)])

    for path in files:
        fname = os.path.basename(path)
        start, mid, end = get_style(fname)
        if not start: continue

        with open(path, 'r', encoding="utf-8") as f:
            lines = f.readlines()

        if not lines: continue

        target = generate_target_header(fname, start, mid, end)
        
        # 1. Identify start point (handle shebang)
        idx = 0
        shebang = None
        if lines[0].startswith("#!"):
            shebang = lines[0].strip()
            idx = 1
            # Skip any empty lines between shebang and header
            while idx < len(lines) and lines[idx].strip() == "":
                idx += 1

        # 2. Smart Comparison (Ignore trailing spaces/slight indent)
        is_perfect = True
        if len(lines) - idx < len(target):
            is_perfect = False
        else:
            for i in range(len(target)):
                if lines[idx + i].strip() != target[i].strip():
                    is_perfect = False
                    break

        if is_perfect:
            print(f"  [OK] {fname}")
            continue

        # 3. If not perfect, Purge and Rebuild
        print(f"  [FIX] Standardizing {fname}")
        
        # Find where the existing IotaOS header block ends
        purge_end = idx
        if idx < len(lines) and (lines[idx].strip().startswith(start.strip()) or lines[idx].strip().startswith(mid.strip())):
            for j in range(idx, min(idx + 15, len(lines))):
                if end.strip() in lines[j] or (start == "# " and not lines[j].startswith("#")):
                    purge_end = j + 1
                    break

        content = lines[purge_end:]
        # Strip leading empty lines from code
        while content and content[0].strip() == "":
            content.pop(0)

        output = []
        if shebang:
            output.append(shebang + "\n\n")
        output.extend(target)
        output.append("\n")
        output.extend(content)

        with open(path, 'w', encoding="utf-8") as f:
            f.writelines(output)

    print("✅ All headers are now idempotent and unified.")

if __name__ == "__main__":
    main()