#!/usr/bin/env python3
"""Audit printable-binary character_map.txt for basic invariants.

Checks:
  - file contains exactly 256 lines
  - all characters are unique
  - reports UTF-8 byte lengths and East Asian Width classification
"""

from __future__ import annotations

import argparse
import unicodedata
from pathlib import Path


def audit(path: Path) -> int:
    lines = [line.rstrip("\n\r") for line in path.read_text(encoding="utf-8").splitlines()]
    errors = 0

    if len(lines) != 256:
        print(f"ERROR: Expected 256 entries, found {len(lines)}")
        errors += 1

    duplicates = {ch for ch in lines if lines.count(ch) > 1}
    if duplicates:
        print(f"ERROR: Duplicate characters detected: {sorted(duplicates)!r}")
        errors += 1

    if errors:
        return errors

    print(f"character map OK (256 unique entries)")
    print("\nIndex | Char | Code point | UTF-8 bytes | Width")
    print("----- | ---- | ---------- | ------------ | -----")
    for idx, ch in enumerate(lines):
        utf8 = ' '.join(f"{b:02X}" for b in ch.encode('utf-8'))
        width = unicodedata.east_asian_width(ch)
        print(f"{idx:5d} | {ch} | U+{ord(ch):04X}    | {utf8:<12} | {width}")

    counts = {}
    for ch in lines:
        counts.setdefault(len(ch.encode('utf-8')), 0)
        counts[len(ch.encode('utf-8'))] += 1

    print("\nUTF-8 length distribution:")
    for length in sorted(counts):
        print(f"  {length}-byte glyphs: {counts[length]}")

    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", nargs="?", default="character_map.txt", type=Path,
                        help="Path to character_map.txt (default: character_map.txt)")
    args = parser.parse_args()
    try:
        return audit(args.path)
    except FileNotFoundError:
        print(f"ERROR: {args.path} not found")
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
