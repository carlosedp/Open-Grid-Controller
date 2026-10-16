# Open Grid: one entry point for the iii firmware and the diii web app.
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

# Formatters run at pinned versions (uvx here, diii/package.json for JS), so
# everyone gets the same output with nothing to install beyond uv and node.
CLANG_FORMAT ?= uvx clang-format@22.1.8
RUFF         ?= uvx ruff@0.14.0
FORMAT_C     := $(wildcard iii-open-grid/src/*.c iii-open-grid/src/*.h)
FORMAT_PY    := tools iii-open-grid/tools

# diii's development tools (Prettier, TypeScript, esbuild) come from
# diii/package.json. The site itself needs no install or build.
NODE     ?= node
DIII_NPM := npm --prefix diii

DIII_UPSTREAM ?= https://github.com/monome/web-diii
DIII_FORK     ?= https://github.com/carlosedp/web-diii
# branch of the fork to push diii/ to, for pull requests to monome/web-diii
DIII_BRANCH   ?= open-grid

# third_party/iii is a git subtree of tehn/iii; our changes are commits on top.
III_UPSTREAM ?= https://codeberg.org/tehn/iii
III_BASE     := $(shell git log -1 --format=%H --grep="git-subtree-dir: third_party/iii")

.PHONY: all help firmware firmware-all upload monitor diii diii-check diii-vendor dist clean \
        format format-check diii-pull-upstream diii-push-fork iii-changes iii-patches iii-pull-upstream

all: firmware

help:
	@echo Firmware, iii-open-grid:
	@echo   make firmware        build for BOARD=$(BOARD)
	@echo   make firmware-all    build for all boards: $(BOARDS)
	@echo   make upload          build and flash BOARD over USB
	@echo   make monitor         open the serial console
	@echo diii web app:
	@echo   make diii            serve diii at http://localhost:$(PORT)
	@echo   make diii-check      type-check diii's standalone modules
	@echo   make diii-vendor     rebuild diii/vendor/ (the code editor) from diii/package.json
	@echo Release:
	@echo   make dist            firmware for all boards + diii into dist/
	@echo   make clean           remove build outputs and dist/
	@echo Code style:
	@echo   make format          format our C, JS and Python in place
	@echo   make format-check    fail if anything is not formatted
	@echo diii subtree, tracking $(DIII_FORK):
	@echo   make diii-pull-upstream   merge monome/web-diii main into diii/
	@echo   make diii-push-fork       push diii/ to the fork, branch DIII_BRANCH, for upstream PRs
	@echo iii subtree, our changes on top of $(III_UPSTREAM):
	@echo   make iii-changes          list our iii commits
	@echo   make iii-patches          export them as patches for tehn/iii into dist/patches/iii
	@echo   make iii-pull-upstream    merge the latest tehn/iii main into third_party/iii
	@echo Options: BOARD=rp2040_zero  PORT=8080

firmware:
	$(PIO) run -e $(BOARD)

firmware-all:
	$(PIO) run $(foreach b,$(BOARDS),-e $(b))

upload:
	$(PIO) run -e $(BOARD) -t upload

monitor:
	$(PIO) device monitor -e $(BOARD)

diii:
	$(NODE) diii/tools/serve.mjs $(PORT)

# --ignore-scripts: esbuild works without its install script, and npm 11 would
# otherwise stop to ask about it
diii/node_modules: diii/package-lock.json
	$(DIII_NPM) ci --ignore-scripts --no-audit --no-fund

diii-check: diii/node_modules
	$(DIII_NPM) run check

diii-vendor: diii/node_modules
	$(DIII_NPM) run vendor

dist: firmware-all
	$(PYTHON) tools/dist.py $(BOARDS)

clean:
	$(PIO) run -t clean $(foreach b,$(BOARDS),-e $(b))
	$(PYTHON) tools/dist.py --clean

format: diii/node_modules
	$(CLANG_FORMAT) -i $(FORMAT_C)
	$(DIII_NPM) run format
	$(RUFF) format $(FORMAT_PY)

format-check: diii/node_modules
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_C)
	$(DIII_NPM) run format:check
	$(RUFF) format --check $(FORMAT_PY)

diii-pull-upstream:
	git subtree pull --prefix=diii $(DIII_UPSTREAM) main

diii-push-fork:
	git subtree push --prefix=diii $(DIII_FORK) $(DIII_BRANCH)

iii-changes:
	git log --oneline $(III_BASE)..HEAD -- third_party/iii

iii-patches:
	git format-patch --relative=third_party/iii -o dist/patches/iii $(III_BASE)..HEAD -- third_party/iii

iii-pull-upstream:
	git subtree pull --prefix=third_party/iii $(III_UPSTREAM) main
