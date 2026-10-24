# Makefile for DigiPools (Digitakt MK1 OS 1.54 elekloader mod)

ELEKLOADER_DIR ?= $(firstword $(wildcard /tmp/ref/elekloader) $(wildcard ../elekloader) $(wildcard ./elekloader))
CROSS ?= m68k-elf-

# Stock Digitakt OS 1.54 firmware syx
# Provide via STOCK_OS=/path/to/Digitakt_OS1.54.syx or place Digitakt_OS1.54.syx in the project root
STOCK_OS ?= $(firstword $(wildcard ./Digitakt_OS1.54.syx) $(wildcard $(HOME)/Downloads/Digitakt_OS1.54_dist/Digitakt_OS1.54.syx) $(wildcard $(HOME)/Downloads/Digitakt_OS1.54.syx))

MOD_NAME = digipools
VERSION = 1.0
OUT_DIR = out
ELEMOD = $(OUT_DIR)/$(MOD_NAME)-$(VERSION).elemod
PATCHED_SYX = $(OUT_DIR)/Digitakt_OS1.54_DigiPools.syx

all: check-stock build patch

check-stock:
	@if [ -z "$(STOCK_OS)" ] || [ ! -f "$(STOCK_OS)" ]; then \
		echo ""; \
		echo "=========================================================================="; \
		echo "ERROR: Stock Digitakt OS 1.54 firmware not found!"; \
		echo ""; \
		echo "Due to copyright, Elektron proprietary firmware is not distributed here."; \
		echo "Please obtain 'Digitakt_OS1.54.syx' from the official Elektron website, and:"; \
		echo "  1. Copy it into this directory as 'Digitakt_OS1.54.syx', OR"; \
		echo "  2. Run: make STOCK_OS=/path/to/Digitakt_OS1.54.syx"; \
		echo "=========================================================================="; \
		echo ""; \
		exit 1; \
	fi

build: check-stock
	@mkdir -p $(OUT_DIR)
	ELEKLOADER_CROSS=$(CROSS) PYTHONPATH=$(ELEKLOADER_DIR) python3 -m elekloader.sdk.build . --stock $(STOCK_OS) --out $(OUT_DIR)

patch: build
	PYTHONPATH=$(ELEKLOADER_DIR) python3 -m elekloader.patch \
		--stock $(STOCK_OS) \
		--mod $(ELEKLOADER_DIR)/mods/core/out/core-4.1-os1.54.elemod \
		--mod $(ELEKLOADER_DIR)/mods/machine-pages/out/machine-pages-1.5-os1.54.elemod \
		--mod $(ELEMOD) \
		--out $(PATCHED_SYX) \
		--version DPLS
	@echo ""
	@echo "=========================================================================="
	@echo "SUCCESS: Modded firmware built at $(PATCHED_SYX)"
	@echo "Flash this file to your Digitakt MK1 using Elektron Transfer or syx tools."
	@echo "=========================================================================="
	@echo ""

clean:
	rm -rf $(OUT_DIR)

.PHONY: all check-stock build patch clean
