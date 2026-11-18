HOSTCC ?= cc
CMAKE ?= cmake
ifeq ($(origin CMAKE),default)
  ifneq ($(wildcard /usr/bin/cmake),)
    CMAKE := /usr/bin/cmake
  endif
endif
VERSION ?= $(shell cat VERSION 2>/dev/null || echo 1.0.0)

DEF_FILES := $(shell find schema/opcodes -type f -name '*.def' 2>/dev/null)
GEN_OUTPUTS := src/gen/skode_dict_gen.h src/gen/skred_opcodes_gen.c doc/REFERENCE.md src/gen/skode-doc.inc src/gen/skred-version.h

SYNTH_DEF_FILES := $(shell find schema/synth -type f -name '*.def' 2>/dev/null)
SYNTH_GEN_OUTPUTS := src/gen/synth_state_gen.h src/gen/synth_alloc_gen.c src/gen/synth_voice_ops_gen.c src/gen/test_schema_bounds_gen.c src/gen/test_synth_equivalence_gen.c

BUILD_DIR ?= build
BUILD_TYPE ?= Release
DIST_DIR ?= dist

DIST_WASM_PLATFORM_ROOT ?= $(DIST_DIR)/wasm
DIST_WASM_PREFIX ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed
DIST_WASM_ARCHIVE ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed.zip
DIST_WASM_RELEASE_ARCHIVE ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed-wasm.zip

DIST_LINUX_PLATFORM_ROOT ?= $(DIST_DIR)/linux-x86_64
DIST_LINUX_PREFIX ?= $(DIST_LINUX_PLATFORM_ROOT)/skred-$(VERSION)-maxed
DIST_LINUX_ARCHIVE ?= $(DIST_LINUX_PLATFORM_ROOT)/skred-$(VERSION)-maxed-linux-x86_64.tar.gz

.PHONY: default help all gen test test-schema native clean wasm wasm-test dist-api dist-api-wasm sync-skode

default: help

help:
	@echo "skred Audio Engine & Skode Runtime Build Targets (v$(VERSION)):"
	@echo "  make help          - Show this list of targets (default)"
	@echo "  make all           - Build native C engine, libraries, and skred / mini-skred CLI"
	@echo "  make gen           - Run declarative compilers (mksynth and mkopcodes)"
	@echo "  make native        - Build native library and executables in $(BUILD_DIR)/"
	@echo "  make test          - Run all schema and engine contract tests"
	@echo "  make test-schema   - Run automated boundary invariance tests"
	@echo "  make wasm          - Compile C DSP engine to WebAssembly (skred_api.js/wasm)"
	@echo "  make wasm-test     - Run headless Node.js tests against WASM engine"
	@echo "  make dist-api      - Package native Linux distribution tarball"
	@echo "  make dist-api-wasm - Package WASM distribution zip archives for releases"
	@echo "  make sync-skode    - Sync Gleam schemas & WASM into ../skode, format & test"
	@echo "  make clean         - Remove generated files, build and dist directories"

all: native

tools/mkopcodes: tools/mkopcodes.c
	$(HOSTCC) -O2 -Wall -Wextra tools/mkopcodes.c -o tools/mkopcodes

$(GEN_OUTPUTS): tools/mkopcodes $(DEF_FILES) VERSION
	mkdir -p src/gen doc
	./tools/mkopcodes schema/opcodes src/gen/skode_dict_gen.h src/gen/skred_opcodes_gen.c doc/REFERENCE.md src/gen/skode-doc.inc
	@v=$$(cat VERSION 2>/dev/null || echo 1.0.0); \
	maj=$$(echo $$v | cut -d. -f1); \
	min=$$(echo $$v | cut -d. -f2); \
	pat=$$(echo $$v | cut -d. -f3 | cut -d- -f1); \
	printf '#ifndef SKRED_VERSION_H\n#define SKRED_VERSION_H\n\n#define SKRED_VERSION "%s"\n#define SKRED_VERSION_MAJOR %s\n#define SKRED_VERSION_MINOR %s\n#define SKRED_VERSION_PATCH %s\n\n#endif /* SKRED_VERSION_H */\n' "$$v" "$${maj:-0}" "$${min:-0}" "$${pat:-0}" > src/gen/skred-version.h

tools/mksynth: tools/mksynth.c
	$(HOSTCC) -O2 -Wall -Wextra tools/mksynth.c -o tools/mksynth

$(SYNTH_GEN_OUTPUTS): tools/mksynth $(SYNTH_DEF_FILES)
	mkdir -p src/gen
	./tools/mksynth schema/synth src/gen $(if $(wildcard $(CURDIR)/../skode),$(CURDIR)/../skode/src/skode/gen,src/gen) $(CURDIR)/VERSION
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

wasm: gen
	./wasm/mkwasm
	@if [ -d wasm ]; then \
		cp -f build_wasm/skred_api.js wasm/skred_api.js 2>/dev/null || true; \
		cp -f build_wasm/skred_api.wasm wasm/skred_api.wasm 2>/dev/null || true; \
	fi

wasm-test:
	node wasm/test-control-dispatch.js build_wasm/skred_api.js

dist-api-wasm: wasm wasm-test
	mkdir -p $(DIST_WASM_PREFIX)
	cp build_wasm/skred_api.js $(DIST_WASM_PREFIX)/
	cp build_wasm/skred_api.wasm $(DIST_WASM_PREFIX)/
	$(CMAKE) -E chdir $(DIST_WASM_PLATFORM_ROOT) $(CMAKE) -E tar cf skred-$(VERSION)-maxed.zip --format=zip skred-$(VERSION)-maxed
	cp $(DIST_WASM_ARCHIVE) $(DIST_WASM_RELEASE_ARCHIVE)
	@echo "==> WASM distribution archives ready at $(DIST_WASM_PLATFORM_ROOT):"
	@ls -la $(DIST_WASM_ARCHIVE) $(DIST_WASM_RELEASE_ARCHIVE)

dist-api: native
	mkdir -p $(DIST_LINUX_PREFIX)
	$(CMAKE) --install $(BUILD_DIR) --prefix $(DIST_LINUX_PREFIX)
	$(CMAKE) -E chdir $(DIST_LINUX_PLATFORM_ROOT) $(CMAKE) -E tar czf skred-$(VERSION)-maxed-linux-x86_64.tar.gz --format=gnutar skred-$(VERSION)-maxed
	@echo "==> Linux distribution archive ready at $(DIST_LINUX_ARCHIVE)"

sync-skode: $(SYNTH_GEN_OUTPUTS) wasm
	@echo "==> Synchronizing Gleam code in skode with skred..."
	./tools/mksynth schema/synth src/gen $(CURDIR)/../skode/src/skode/gen $(CURDIR)/VERSION
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
	rm -rf $(BUILD_DIR) build_wasm $(DIST_DIR) src/gen/test_schema_bounds tools/mksynth tools/mkopcodes *-history.txt .skred_history
