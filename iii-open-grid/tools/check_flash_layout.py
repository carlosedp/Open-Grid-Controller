"""Post-build check: the program must end below the pinned status sector.

The status sector (mode) and the filesystem sit at a fixed flash offset
(III_FLASH_STATUS_OFFSET, see iii's flash.c), so firmware updates don't move
the files. A program that grows into it would make iii fall back to placing
them right after the program, which moves them; fail the build instead.

usage: check_flash_layout.py <nm> <elf> <status offset>
"""

import subprocess
import sys

FLASH_BASE = 0x10000000


def main() -> None:
    nm, elf, limit_arg = sys.argv[1:4]
    limit = int(limit_arg, 0)
    symbols = subprocess.run([nm, elf], capture_output=True, text=True, check=True).stdout
    end = next(
        int(line.split()[0], 16) - FLASH_BASE
        for line in symbols.splitlines()
        if line.endswith(" __flash_binary_end")
    )
    room = limit - end
    if room <= 0:
        sys.exit(
            f"flash layout: program ends at 0x{end:x}, past the status sector at 0x{limit:x}; "
            "raise OG_FLASH_STATUS_OFFSET (moves the filesystem once) or shrink the program"
        )
    print(
        f"flash layout: program ends at 0x{end:x}, status sector at 0x{limit:x}, "
        f"filesystem from 0x{limit + 0x10000:x} ({room // 1024} KB to spare)"
    )


if __name__ == "__main__":
    main()
