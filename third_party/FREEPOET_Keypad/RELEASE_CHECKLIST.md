# Release Checklist

Use this checklist before publishing a zip file or a tagged repository release.

## Metadata

- [ ] `library.properties:name` is final
- [ ] `library.properties:version` matches the release tag
- [ ] `library.properties:url` points to the public repository or product page
- [ ] `author` and `maintainer` fields are correct for the release owner
- [ ] `includes=FREEPOET_keypad.h` is present

## Documentation

- [ ] `README.md` matches the current protocol and API
- [ ] `CHANGELOG.md` includes the new release entry
- [ ] `DEPENDENCIES.md` reflects current dependency state
- [ ] `LICENSE` matches the intended license

## Validation

- [ ] At least one example compiles in Arduino IDE
- [ ] Basic device info reads succeed on hardware
- [ ] `setLevel()` and `showLeds()` work on hardware
- [ ] Packed writes (`setLevelBlockPacked()` or `setFullFrame()`) are tested on hardware
- [ ] Key read APIs still work under LED update load

## Packaging

- [ ] Zip root folder is `FREEPOET_keypad/`
- [ ] Release package contains `src/`, `examples/`, `library.properties`, `keywords.txt`, `README.md`, `CHANGELOG.md`, `DEPENDENCIES.md`, `RELEASE_CHECKLIST.md`, and `LICENSE`
- [ ] No editor cache or build artifacts are included