# Vendored third-party code

Copied in-tree (not git submodules) so the repo builds on its own. Each
directory is an unmodified export of the upstream commit below; local changes
belong in our own code (`iii-open-grid/`), not here. To update one, re-export
it at a new commit and update this table.

| Directory          | Upstream                                                  | Commit    | License | Notes                                         |
| ------------------ | --------------------------------------------------------- | --------- | ------- | --------------------------------------------- |
| `iii/`             | https://codeberg.org/tehn/iii                             | `ed920ef` | GPL-3.0 | v1.1.2, same commit iii-grid-2022 pins         |
| `lua/`             | https://codeberg.org/tehn/lua                             | `86c9ee5` | MIT     | monome's Lua 5.4 fork (32-bit config)          |
| `littlefs/`        | https://codeberg.org/tehn/littlefs                        | `e3eefe8` | BSD-3   | only `lfs*.{c,h}`, license and readme kept     |
| `FREEPOET_Keypad/` | https://github.com/FREEPOET-OFFICIAL/FREEPOET_Keypad      | `00893c1` | MIT     | Arduino library; register reference for our C driver |

Not vendored: **pico-sdk 2.2.0** (large, with its own submodules). PlatformIO
installs it (`framework-picosdk`), or point CMake at a checkout with
`PICO_SDK_PATH`.

Re-export example (run from the repo root, in a clone of the upstream repo at `$UP`):

```sh
rm -rf third_party/iii && mkdir third_party/iii
git -C "$UP" archive <commit> | tar -x -C third_party/iii
```

**Licensing:** iii is GPL-3.0, so firmware images built from `iii-open-grid/`
are GPL-3.0 as a whole.
