#!/usr/bin/env python3

# 
# IotaOS - osDump.py
# Copyright (c) 2026 grish-ka
# Licensed under the MIT License.
#

import os
import datetime

# Configuration
OUTPUT_FILE = "os_dump.txt"
DIRECTORIES_TO_SCAN = ["src"]
EXPLICIT_FILES = ["Makefile", "grub.cfg"]

def get_markdown_lang(filename):
    if filename == "Makefile": return "makefile"
    if filename.endswith(".cfg"): return "text"
    if filename.endswith((".c", ".h")): return "c"
    if filename.endswith(".s"): return "assembly"
    return "text"

def main():
    print(f"Generating OS Context Dump...")
    
    # Gather all files
    all_files = EXPLICIT_FILES.copy()
    
    for directory in DIRECTORIES_TO_SCAN:
        if os.path.exists(directory):
            for root, _, files in os.walk(directory):
                for file in files:
                    if file.endswith(('.c', '.h', '.s', '.ld')):
                        all_files.append(os.path.join(root, file))
        else:
            print(f"Warning: Directory '{directory}' not found.")

    # Write to output file
    with open(OUTPUT_FILE, "w", encoding="utf-8") as out:
        out.write("### [IOTAOS PROJECT SAVE STATE]\n")
        out.write(f"Generated: {datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        for file_path in all_files:
            # Standardize path slashes for cross-platform neatness
            clean_path = file_path.replace("\\", "/") 
            
            if os.path.exists(file_path):
                out.write(f"### {clean_path}\n")
                out.write(f"```{get_markdown_lang(file_path)}\n")
                
                try:
                    with open(file_path, "r", encoding="utf-8") as f:
                        content = f.read()
                        out.write(content)
                        # Ensure the code block ends on a new line
                        if not content.endswith("\n"):
                            out.write("\n")
                except Exception as e:
                    out.write(f"// Error reading file: {e}\n")
                    
                out.write("```\n\n")
            else:
                print(f"Skipped {file_path} (File not found)")

    print(f"Done! 100% of your code is now in -> {OUTPUT_FILE}")

if __name__ == "__main__":
    main()