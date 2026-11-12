#!/usr/bin/env python3
"""Collect release artifacts into dist/ (called by the root Makefile).

dist/
  firmware/iii-open-grid-<version>-<board>.uf2   from the PlatformIO builds

diii, the web app, lives in its own repo (github.com/carlosedp/web-diii),
which publishes itself.

usage: dist.py BOARD...
       dist.py --clean
"""

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"


def firmware_version() -> str:
    main_c = (ROOT / "firmware" / "src" / "main.c").read_text(encoding="utf-8")
    match = re.search(r'#define DEVICE_VERSION_BASE "([^"]+)"', main_c)
    return match.group(1) if match else "unknown"


def copy_firmware(boards) -> None:
    out = DIST / "firmware"
    out.mkdir(parents=True, exist_ok=True)
    version = firmware_version()
    for board in boards:
        uf2 = ROOT / ".pio" / "build" / board / "firmware.uf2"
        if not uf2.is_file():
            sys.exit(f"missing {uf2.relative_to(ROOT)}; build it first (make firmware-all)")
        target = out / f"iii-open-grid-{version}-{board}.uf2"
        shutil.copyfile(uf2, target)
        print(f"  {target.relative_to(ROOT)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("boards", nargs="*")
    parser.add_argument("--clean", action="store_true")
    parser.add_argument(
        "--version", action="store_true", help="print the firmware version (make release)"
    )
    args = parser.parse_args()

    if args.version:
        print(firmware_version())
        return
    if args.clean:
        shutil.rmtree(DIST, ignore_errors=True)
        return
    print("dist:")
    copy_firmware(args.boards)


if __name__ == "__main__":
    main()
