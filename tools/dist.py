#!/usr/bin/env python3
"""Collect release artifacts into dist/ (called by the root Makefile).

dist/
  firmware/iii-open-grid-<version>-<board>.uf2   from the PlatformIO builds
  diii/                                          the web app, ready to host

usage: dist.py [--env-suffix _offline] BOARD...
       dist.py --clean
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DIST = ROOT / "dist"
DIII_SKIP = {"docs", "README.md", "README-development.md", ".gitignore", ".github", ".clang-format"}


def firmware_version() -> str:
    main_c = (ROOT / "iii-open-grid" / "src" / "main.c").read_text(encoding="utf-8")
    match = re.search(r'#define DEVICE_VERSION "([^"]+)"', main_c)
    return match.group(1) if match else "unknown"


def git_revision() -> str:
    try:
        rev = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=ROOT,
                             capture_output=True, text=True, check=True).stdout.strip()
        dirty = subprocess.run(["git", "status", "--porcelain", "--", "diii"], cwd=ROOT,
                               capture_output=True, text=True, check=True).stdout.strip()
        return rev + ("-dirty" if dirty else "")
    except (OSError, subprocess.CalledProcessError):
        return "dev"


def copy_firmware(boards, env_suffix) -> None:
    out = DIST / "firmware"
    out.mkdir(parents=True, exist_ok=True)
    version = firmware_version()
    for board in boards:
        uf2 = ROOT / ".pio" / "build" / f"{board}{env_suffix}" / "firmware.uf2"
        if not uf2.is_file():
            sys.exit(f"missing {uf2.relative_to(ROOT)}; build it first (make firmware-all)")
        target = out / f"iii-open-grid-{version}-{board}.uf2"
        shutil.copyfile(uf2, target)
        print(f"  {target.relative_to(ROOT)}")


def copy_diii() -> None:
    out = DIST / "diii"
    shutil.rmtree(out, ignore_errors=True)
    shutil.copytree(ROOT / "diii", out, ignore=lambda _dir, names: [n for n in names if n in DIII_SKIP])

    # A new cache name per build makes installed copies pick up the release.
    sw = out / "sw.js"
    text = sw.read_text(encoding="utf-8")
    text, count = re.subn(r"const CACHE = '[^']*';", f"const CACHE = 'diii-{git_revision()}';", text)
    if count != 1:
        sys.exit("could not stamp the cache name in diii/sw.js")
    sw.write_text(text, encoding="utf-8", newline="\n")
    print(f"  {out.relative_to(ROOT)}/ (cache diii-{git_revision()})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("boards", nargs="*")
    parser.add_argument("--env-suffix", default="")
    parser.add_argument("--clean", action="store_true")
    args = parser.parse_args()

    if args.clean:
        shutil.rmtree(DIST, ignore_errors=True)
        return
    print("dist:")
    copy_firmware(args.boards, args.env_suffix)
    copy_diii()


if __name__ == "__main__":
    main()
