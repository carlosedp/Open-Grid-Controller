#!/usr/bin/env python3
"""Embed a Lua file as a C byte array (same layout as iii's resource/lib_lua.c).

usage: embed_lua.py <input.lua> <output.c>
"""

import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src = Path(sys.argv[1]).read_bytes()
    data = src + b"\0"
    body = ",".join(f"0x{b:02x}" for b in data)
    Path(sys.argv[2]).write_text(
        '#include "lib_lua.h"\n'
        f"unsigned const char lib_lua_data[] = {{{body} }};\n"
        f"unsigned const int lib_lua_size = {len(data)};\n",
        newline="\n",
    )


if __name__ == "__main__":
    main()
