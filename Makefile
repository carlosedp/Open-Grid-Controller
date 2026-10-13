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
# The *_offline PlatformIO envs reuse the already-installed Arduino-Pico
# toolchain. Use `make OFFLINE=` to build with the full toolchain instead.
OFFLINE ?= _offline
PORT    ?= 8000

DIII_UPSTREAM ?= https://github.com/monome/web-diii
DIII_FORK     ?= https://github.com/carlosedp/web-diii
# branch of the fork to push diii/ to, for pull requests to monome/web-diii
DIII_BRANCH   ?= open-grid

# third_party/iii is a git subtree of tehn/iii; our changes are commits on top.
III_UPSTREAM ?= https://codeberg.org/tehn/iii
III_BASE     := $(shell git log -1 --format=%H --grep="git-subtree-dir: third_party/iii")

ENV := $(BOARD)$(OFFLINE)

.PHONY: all help firmware firmware-all upload monitor diii dist clean \
        diii-pull-upstream diii-push-fork iii-changes iii-patches iii-pull-upstream

all: firmware

help:
	@echo Firmware, iii-open-grid:
	@echo   make firmware        build for BOARD=$(BOARD), env $(ENV)
	@echo   make firmware-all    build for all boards: $(BOARDS)
	@echo   make upload          build and flash BOARD over USB
	@echo   make monitor         open the serial console
	@echo diii web app:
	@echo   make diii            serve diii at http://localhost:$(PORT)
	@echo Release:
	@echo   make dist            firmware for all boards + diii into dist/
	@echo   make clean           remove build outputs and dist/
	@echo diii subtree, tracking $(DIII_FORK):
	@echo   make diii-pull-upstream   merge monome/web-diii main into diii/
	@echo   make diii-push-fork       push diii/ to the fork, branch DIII_BRANCH, for upstream PRs
	@echo iii subtree, our changes on top of $(III_UPSTREAM):
	@echo   make iii-changes          list our iii commits
	@echo   make iii-patches          export them as patches for tehn/iii into dist/patches/iii
	@echo   make iii-pull-upstream    merge the latest tehn/iii main into third_party/iii
	@echo Options: BOARD=rp2040_zero  OFFLINE=  PORT=8080

firmware:
	$(PIO) run -e $(ENV)

firmware-all:
	$(PIO) run $(foreach b,$(BOARDS),-e $(b)$(OFFLINE))

upload:
	$(PIO) run -e $(ENV) -t upload

monitor:
	$(PIO) device monitor -e $(ENV)

diii:
	$(PYTHON) -m http.server $(PORT) --bind 127.0.0.1 --directory diii

dist: firmware-all
	$(PYTHON) tools/dist.py --env-suffix "$(OFFLINE)" $(BOARDS)

clean:
	$(PIO) run -t clean $(foreach b,$(BOARDS),-e $(b)$(OFFLINE))
	$(PYTHON) tools/dist.py --clean

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
