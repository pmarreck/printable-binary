#!/usr/bin/env python3
"""Generate character_map_embedded.h from character_map.txt."""
from __future__ import annotations
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "character_map.txt"
OUTPUT_PATH = ROOT / "character_map_embedded.h"

def main() -> None:
    lines = MAP_PATH.read_text(encoding="utf-8").splitlines()
    if len(lines) != 256:
        raise SystemExit(f"expected 256 lines in {MAP_PATH}, got {len(lines)}")

    with OUTPUT_PATH.open("w", encoding="utf-8") as fh:
        fh.write("#pragma once\n")
        fh.write("// Auto-generated from character_map.txt to support embedded builds\n")
        fh.write("static const char *embedded_character_map[256] = {\n")
        for line in lines:
            fh.write(f"    {json.dumps(line)},\n")
        fh.write("};\n")

    print(f"Wrote {OUTPUT_PATH.relative_to(ROOT)}")

if __name__ == "__main__":
    main()
