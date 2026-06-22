#!/usr/bin/env python3
"""Build Telltale module source files."""
import os

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "src")

def w(name, content):
    path = os.path.join(SRC, name)
    with open(path, "w") as f:
        f.write(content)
    print(f"{name}: {len(content.splitlines())} lines")

# diff_engine.cpp - core implementation
w("diff_engine.cpp", open(os.path.join(ROOT, "scripts", "diff_engine.cpp.part")).read() if False else "")
