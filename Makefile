HOSTCC ?= cc
CMAKE ?= cmake

DEF_FILES := $(shell find doc/opcodes -type f -name '*.def' 2>/dev/null)
GEN_OUTPUTS := src/gen/skode_dict_gen.h src/gen/skred_opcodes_gen.c doc/REFERENCE.md src/gen/skode-doc.inc

SYNTH_DEF_FILES := $(shell find schema -type f -name '*.def' 2>/dev/null)
SYNTH_GEN_OUTPUTS := src/gen/synth_state_gen.h src/gen/synth_alloc_gen.c src/gen/synth_voice_ops_gen.c src/gen/test_schema_bounds_gen.c src/gen/test_synth_equivalence_gen.c

BUILD_DIR ?= build
BUILD_TYPE ?= Release

.PHONY: default help all gen test test-schema native clean wasm wasm-test sync-skode

default: help

help:
	@echo "skred Audio Engine & Skode Runtime Build Targets (v1.0.0):"
	@echo "  make help        - Show this list of targets (default)"
	@echo "  make all         - Build native C engine, libraries, and skred / mini-skred CLI"
	@echo "  make gen         - Run declarative compilers (mksynth and mkopcodes)"
	@echo "  make native      - Build native library and executables in $(BUILD_DIR)/"
	@echo "  make test        - Run all schema and engine contract tests"
	@echo "  make test-schema - Run automated boundary invariance tests"
	@echo "  make wasm        - Compile C DSP engine to WebAssembly (skred_api.js/wasm)"
	@echo "  make wasm-test   - Run headless Node.js tests against WASM engine"
	@echo "  make sync-skode  - Sync Gleam schemas & WASM into ../skode, format & test"
	@echo "  make clean       - Remove generated files and build directories"

all: native

tools/mkopcodes: tools/mkopcodes.c
	$(HOSTCC) -O2 -Wall -Wextra tools/mkopcodes.c -o tools/mkopcodes

$(GEN_OUTPUTS): tools/mkopcodes $(DEF_FILES)
	mkdir -p src/gen doc
	./tools/mkopcodes doc/opcodes src/gen/skode_dict_gen.h src/gen/skred_opcodes_gen.c doc/REFERENCE.md src/gen/skode-doc.inc

tools/mksynth: tools/mksynth.c
	$(HOSTCC) -O2 -Wall -Wextra tools/mksynth.c -o tools/mksynth

$(SYNTH_GEN_OUTPUTS): tools/mksynth $(SYNTH_DEF_FILES)
	mkdir -p src/gen
	./tools/mksynth schema src/gen $(if $(wildcard $(CURDIR)/../skode),$(CURDIR)/../skode/src/skode/gen,src/gen) $(CURDIR)/VERSION
	@if command -v gleam >/dev/null 2>&1 && [ -d "$(CURDIR)/../skode" ]; then \
		(cd $(CURDIR)/../skode && gleam format >/dev/null 2>&1 || true); \
	fi

gen: $(GEN_OUTPUTS) $(SYNTH_GEN_OUTPUTS)

test-schema: $(SYNTH_GEN_OUTPUTS)
	$(HOSTCC) -O2 -Wall -Wextra -Werror src/gen/test_schema_bounds_gen.c -o src/gen/test_schema_bounds
	./src/gen/test_schema_bounds

native: gen
	$(CMAKE) -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	$(CMAKE) --build $(BUILD_DIR)

test: gen test-schema native
	ctest --test-dir $(BUILD_DIR) --output-on-failure

wasm: gen native
	./wasm/mkwasm
	@if [ -d wasm ]; then \
		cp -f build_wasm/skred_api.js wasm/skred_api.js 2>/dev/null || true; \
		cp -f build_wasm/skred_api.wasm wasm/skred_api.wasm 2>/dev/null || true; \
	fi

wasm-test:
	node wasm/test-control-dispatch.js build_wasm/skred_api.js

sync-skode: $(SYNTH_GEN_OUTPUTS) wasm
	@echo "==> Synchronizing Gleam code in skode with skred..."
	./tools/mksynth schema src/gen $(CURDIR)/../skode/src/skode/gen $(CURDIR)/VERSION
	@if command -v gleam >/dev/null 2>&1 && [ -d "$(CURDIR)/../skode" ]; then \
		(cd $(CURDIR)/../skode && gleam format >/dev/null 2>&1); \
	fi
	@echo "==> Copying compiled WASM binary to skode editor..."
	mkdir -p $(CURDIR)/../skode/app/editor/wasm
	cp -f build_wasm/skred_api.js $(CURDIR)/../skode/app/editor/wasm/
	cp -f build_wasm/skred_api.wasm $(CURDIR)/../skode/app/editor/wasm/
	@echo "==> Running Skode tests..."
	@if command -v gleam >/dev/null 2>&1 && [ -d "$(CURDIR)/../skode" ]; then \
		(cd $(CURDIR)/../skode && gleam test && gleam test --target javascript); \
	fi
	@echo "==> skred and skode are synchronized!"

clean:
	rm -rf $(BUILD_DIR) build_wasm src/gen/test_schema_bounds tools/mksynth tools/mkopcodes
