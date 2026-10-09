# Changelog

All notable changes to this library are documented in this file.

## 2.1.0 - 2026-04-09

- Renamed the published Arduino library package to `FREEPOET_keypad`.
- Renamed the primary header to `FREEPOET_keypad.h` and the primary class to `FREEPOET_keypad`.
- Kept a compatibility type alias for sketches that still reference `PadGrid8x8` after including the new header.
- Updated examples, metadata, and release documents to use the new package name.

## 2.0.0 - 2026-04-09

- Updated the library to match 8x8-Pad IIC protocol `0x02`.
- Added preferred LED APIs for `level`, `level block`, packed block, 8x8 block, and full-frame writes.
- Kept legacy RGB methods as compatibility helpers that quantize to grayscale level.
- Added automatic fallback for large frame writes on Arduino cores with small `Wire` buffers.
- Updated examples, keywords, and README for level-based buffered LED semantics.
- Confirmed there is no external third-party Arduino library dependency.

## 1.0.0

- Initial Arduino library release with key read support and legacy RGB-oriented LED APIs.