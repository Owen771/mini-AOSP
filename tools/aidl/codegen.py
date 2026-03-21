#!/usr/bin/env python3
"""
mini-AOSP AIDL code generator (placeholder for Stage 0)

In Stage 2, this will parse .aidl files and generate:
- Kotlin proxy/stub classes
- C++ proxy/stub classes
- Parcel serialization code

Usage: python3 codegen.py <input.aidl> --lang kotlin --out <output_dir>
"""

import sys

def main():
    print("AIDL codegen placeholder — not yet implemented (Stage 2)")
    print("Usage: codegen.py <input.aidl> --lang [kotlin|cpp] --out <dir>")
    if len(sys.argv) > 1:
        print(f"  Input: {sys.argv[1]}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
