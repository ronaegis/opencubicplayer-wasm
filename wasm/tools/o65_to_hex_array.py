#!/usr/bin/env python3
"""Convert a binary blob into the hexdump format expected by libsidplayfp."""
from __future__ import annotations
import pathlib
import sys

def main() -> int:
    if len(sys.argv) != 3:
        sys.stderr.write("usage: o65_to_hex_array.py <input> <output>\n")
        return 1

    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])

    data = src.read_bytes()

    # Ensure parent directory exists to avoid race conditions in CMake custom commands.
    dst.parent.mkdir(parents=True, exist_ok=True)

    with dst.open('w', encoding='ascii') as handle:
        for byte in data:
            handle.write(f"0x{byte:02X},\n")

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
