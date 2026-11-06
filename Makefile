# Open Grid: one entry point for the iii firmware. diii, the web app, lives in
# its own repo (github.com/carlosedp/web-diii); `make diii` serves a checkout of it.
# Recipes only call pio, python and git, so they run the same from
# PowerShell, cmd or Git Bash.
#
#   make                  build the firmware for $(BOARD)
#   make help             list all targets

PIO     ?= pio
PYTHON  ?= python
BOARD   ?= xiao_rp2040
BOARDS  := xiao_rp2040 rp2040_zero
PORT    ?= 8000

# Formatters run at pinned versions (uvx), so everyone gets the same output
# with nothing to install beyond uv.
CLANG_FORMAT ?= uvx clang-format@22.1.8
RUFF         ?= uvx ruff@0.14.0
FORMAT_C     := $(wildcard iii-open-grid/src/*.c iii-open-grid/src/*.h)
FORMAT_PY    := tools iii-open-grid/tools

# A checkout of github.com/carlosedp/web-diii, for `make diii`
NODE     ?= node
DIII_DIR ?= ../web-diii

# third_party/iii is a git subtree of tehn/iii; our changes are commits on top.
III_UPSTREAM ?= https://codeberg.org/tehn/iii
III_BASE     := $(shell git log -1 --format=%H --grep="git-subtree-dir: third_party/iii")

.PHONY: all help firmware firmware-all upload monitor diii dist release clean \
        format format-check iii-changes iii-patches iii-pull-upstream

all: firmware

help:
	@echo Firmware, iii-open-grid:
	@echo   make firmware        build for BOARD=$(BOARD)
	@echo   make firmware-all    build for all boards: $(BOARDS)
	@echo   make upload          build and flash BOARD over USB
	@echo   make monitor         open the serial console
	@echo diii web app, a checkout in DIII_DIR=$(DIII_DIR):
	@echo   make diii            serve diii at http://localhost:$(PORT)
	@echo Release:
	@echo   make dist            firmware for all boards into dist/
	@echo   make release         dist, then a GitHub release with the UF2s (diii offers it as an update)
	@echo   make clean           remove build outputs and dist/
	@echo Code style:
	@echo   make format          format our C and Python in place
	@echo   make format-check    fail if anything is not formatted
	@echo iii subtree, our changes on top of $(III_UPSTREAM):
	@echo   make iii-changes          list our iii commits
	@echo   make iii-patches          export them as patches for tehn/iii into dist/patches/iii
	@echo   make iii-pull-upstream    merge the latest tehn/iii main into third_party/iii
	@echo Options: BOARD=rp2040_zero  PORT=8080  DIII_DIR=../web-diii

firmware:
	$(PIO) run -e $(BOARD)

firmware-all:
	$(PIO) run $(foreach b,$(BOARDS),-e $(b))

upload:
	$(PIO) run -e $(BOARD) -t upload

monitor:
	$(PIO) device monitor -e $(BOARD)

diii:
	$(NODE) $(DIII_DIR)/tools/serve.mjs $(PORT)

dist: firmware-all
	$(PYTHON) tools/dist.py $(BOARDS)

# Publishes the firmware: a GitHub release tagged with the version in main.c,
# carrying the UF2s; diii's "firmware" dialog finds it there. Needs gh, logged
# in, and the commit pushed (the tag is made on the remote's main).
release: dist
	gh release create $(shell $(PYTHON) tools/dist.py --version) \
		$(wildcard dist/firmware/iii-open-grid-$(shell $(PYTHON) tools/dist.py --version)-*.uf2) \
		--target main --title $(shell $(PYTHON) tools/dist.py --version) --generate-notes

clean:
	$(PIO) run -t clean $(foreach b,$(BOARDS),-e $(b))
	$(PYTHON) tools/dist.py --clean

format:
	$(CLANG_FORMAT) -i $(FORMAT_C)
	$(RUFF) format $(FORMAT_PY)

format-check:
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_C)
	$(RUFF) format --check $(FORMAT_PY)

iii-changes:
	git log --oneline $(III_BASE)..HEAD -- third_party/iii

iii-patches:
	git format-patch --relative=third_party/iii -o dist/patches/iii $(III_BASE)..HEAD -- third_party/iii

iii-pull-upstream:
	git subtree pull --prefix=third_party/iii $(III_UPSTREAM) main
