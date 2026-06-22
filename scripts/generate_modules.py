#!/usr/bin/env python3
"""Generate large Telltale implementation modules."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
INC = os.path.join(ROOT, "include", "telltale")
SRC = os.path.join(ROOT, "src")

def write(path, content):
    with open(path, "w") as f:
        f.write(content)
    print(f"Wrote {path}: {len(content.splitlines())} lines")

# Files will be written by separate invocations below
print("Use direct file writes")
