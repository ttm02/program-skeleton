#!/usr/bin/env python3

import os
import sys
from pathlib import Path

def count_lines_in_file(file_path):
    """Count non-empty lines in a file."""
    try:
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            return sum(1 for line in f if line.strip())
    except (IOError, OSError):
        return 0

def traverse_and_count(folder_path):
    """Traverse folder and count lines in .c and .h files."""
    if not os.path.isdir(folder_path):
        print(f"Error: '{folder_path}' is not a valid directory")
        return
    
    total_lines = 0
    file_count = 0
    
    folder = Path(folder_path)
    
    # Find all .c and .h files recursively
    for file_path in folder.rglob('*'):
        if file_path.suffix.lower() in ['.c', '.h'] and file_path.is_file():
            lines = count_lines_in_file(file_path)
            total_lines += lines
            file_count += 1
            print(f"{file_path.relative_to(folder)}: {lines} lines")
    
    print(f"\nTotal: {file_count} files, {total_lines} lines of code")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python script.py <folder_path>")
        sys.exit(1)
    
    folder_path = sys.argv[1]
    traverse_and_count(folder_path)