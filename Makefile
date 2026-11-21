HOSTCC ?= cc
CMAKE ?= cmake
ifeq ($(origin CMAKE),default)
  ifneq ($(wildcard /usr/bin/cmake),)
    CMAKE := /usr/bin/cmake
  endif
endif
NODE ?= $(if $(EMSDK_NODE),$(EMSDK_NODE),$(shell which node 2>/dev/null || echo node))
VERSION ?= $(shell cat VERSION 2>/dev/null || echo 1.0.0)

DEF_FILES := $(shell find schema/opcodes -type f -name '*.def' 2>/dev/null)
GEN_OUTPUTS := src/gen/skode_dict_gen.h src/gen/skred_opcodes_gen.c doc/REFERENCE.md src/gen/skode-doc.inc src/gen/skred-version.h

SYNTH_DEF_FILES := $(shell find schema/synth -type f -name '*.def' 2>/dev/null)
SYNTH_GEN_OUTPUTS := src/gen/synth_state_gen.h src/gen/synth_alloc_gen.c src/gen/synth_voice_ops_gen.c src/gen/test_schema_bounds_gen.c src/gen/test_synth_equivalence_gen.c

BUILD_DIR ?= build
BUILD_TYPE ?= Release
DIST_DIR ?= dist

HOST_OS ?= $(shell uname -s | tr '[:upper:]' '[:lower:]')
HOST_ARCH ?= $(shell uname -m)
MACOS_UNIVERSAL ?= 0
MACOS_UNIVERSAL_ARCHS ?= x86_64;arm64
DIST_PLATFORM ?= $(if $(filter 1 true yes,$(MACOS_UNIVERSAL)),macos-universal,$(HOST_OS)-$(HOST_ARCH))
DIST_PLATFORM_ROOT ?= $(DIST_DIR)/$(DIST_PLATFORM)
DIST_PREFIX ?= $(DIST_PLATFORM_ROOT)/skred-$(VERSION)-maxed
CMAKE_PLATFORM_ARGS ?= $(if $(filter 1 true yes,$(MACOS_UNIVERSAL)),-DCMAKE_OSX_ARCHITECTURES="$(MACOS_UNIVERSAL_ARCHS)",)

ZIG_LOCAL_CACHE_DIR ?= /tmp/sk3-zig-cache
ZIG_GLOBAL_CACHE_DIR ?= /tmp/sk3-zig-global-cache
ZIG_ENV = $(CMAKE) -E env ZIG_LOCAL_CACHE_DIR=$(ZIG_LOCAL_CACHE_DIR) ZIG_GLOBAL_CACHE_DIR=$(ZIG_GLOBAL_CACHE_DIR)

DIST_WASM_PLATFORM_ROOT ?= $(DIST_DIR)/wasm
DIST_WASM_PREFIX ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed
DIST_WASM_ARCHIVE ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed.zip
DIST_WASM_RELEASE_ARCHIVE ?= $(DIST_WASM_PLATFORM_ROOT)/skred-$(VERSION)-maxed-wasm.zip

DIST_LINUX_PLATFORM_ROOT ?= $(DIST_DIR)/linux-x86_64
DIST_LINUX_PREFIX ?= $(DIST_LINUX_PLATFORM_ROOT)/skred-$(VERSION)-maxed
DIST_LINUX_ARCHIVE ?= $(DIST_LINUX_PLATFORM_ROOT)/skred-$(VERSION)-maxed-linux-x86_64.tar.gz

.PHONY: default help all gen test test-schema native gui clean wasm wasm-test dist-api dist-api-wasm dist-api-linux-armhf dist-api-linux-aarch64 dist-api-windows windows-cross-libs sync-skode dsp-demos dsp-experiments

default: help

help:
	@echo "sk3 Audio Engine & Skode Runtime Build Targets (v$(VERSION)):"
	@echo "  make help          - Show this list of targets (default)"
	@echo "  make all           - Build native C engine, libraries, and sk3 CLI"
	@echo "  make gen           - Run declarative compilers (mksynth and mkopcodes)"
	@echo "  make native        - Build native library and executables in $(BUILD_DIR)/"
	@echo "  make gui           - Build native sk3-gui executable in $(BUILD_DIR)/"
	@echo "  make dsp-demos     - Build & render all standalone DSP audio demo WAV files"
	@echo "  make dsp-experiments - Build all standalone DSP toolkit experiment binaries"
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
	$(CMAKE) -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_PLATFORM_ARGS)
	$(CMAKE) --build $(BUILD_DIR)

gui: gen
	$(CMAKE) -B $(BUILD_DIR) -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_PLATFORM_ARGS)
	$(CMAKE) --build $(BUILD_DIR) --target sk3-gui

dsp-experiments: native
	$(CMAKE) --build $(BUILD_DIR) --target dsp_experiments

dsp-demos: native
	$(CMAKE) --build $(BUILD_DIR) --target run_dsp_demos

test: gen test-schema native
	ctest --test-dir $(BUILD_DIR) --output-on-failure

wasm: gen
	./wasm/mkwasm
	@if [ -d wasm ]; then \
		cp -f build_wasm/skred_api.js wasm/skred_api.js 2>/dev/null || true; \
		cp -f build_wasm/skred_api.wasm wasm/skred_api.wasm 2>/dev/null || true; \
	fi

wasm-test:
	$(NODE) wasm/test-control-dispatch.js build_wasm/skred_api.js

dist-api-wasm: wasm wasm-test
	mkdir -p $(DIST_WASM_PREFIX)
	cp build_wasm/skred_api.js $(DIST_WASM_PREFIX)/
	cp build_wasm/skred_api.wasm $(DIST_WASM_PREFIX)/
	cp build_wasm/skred_worklet_engine.js $(DIST_WASM_PREFIX)/
	cp build_wasm/skred_worklet_engine.wasm $(DIST_WASM_PREFIX)/
	cp build_wasm/skred_worklet_processor.js $(DIST_WASM_PREFIX)/
	$(CMAKE) -E chdir $(DIST_WASM_PLATFORM_ROOT) $(CMAKE) -E tar cf skred-$(VERSION)-maxed.zip --format=zip skred-$(VERSION)-maxed
	cp $(DIST_WASM_ARCHIVE) $(DIST_WASM_RELEASE_ARCHIVE)
	@echo "==> WASM distribution archives ready at $(DIST_WASM_PLATFORM_ROOT):"
	@ls -la $(DIST_WASM_ARCHIVE) $(DIST_WASM_RELEASE_ARCHIVE)

dist-api: native
	mkdir -p $(DIST_PREFIX)
	$(CMAKE) --install $(BUILD_DIR) --prefix $(DIST_PREFIX)
	@if [ "$(DIST_PLATFORM)" = "macos-universal" ]; then \
		rm -rf $(DIST_PREFIX)/sk3.app; \
		mkdir -p $(DIST_PREFIX)/sk3.app/Contents/MacOS $(DIST_PREFIX)/sk3.app/Contents/Resources; \
		sed "s/VERSION_PLACEHOLDER/$(VERSION)/g" assets/macos/Info.plist > $(DIST_PREFIX)/sk3.app/Contents/Info.plist; \
		cp $(DIST_PREFIX)/bin/sk3-gui $(DIST_PREFIX)/sk3.app/Contents/MacOS/; \
		cp assets/osko.icns $(DIST_PREFIX)/sk3.app/Contents/Resources/; \
		chmod +x $(DIST_PREFIX)/sk3.app/Contents/MacOS/sk3-gui; \
		if command -v xattr >/dev/null 2>&1; then xattr -cr $(DIST_PREFIX)/sk3.app; fi; \
		if command -v codesign >/dev/null 2>&1; then codesign --force --deep --sign - --entitlements assets/macos/entitlements.plist $(DIST_PREFIX)/sk3.app; fi; \
		$(CMAKE) -E chdir $(DIST_PLATFORM_ROOT) $(CMAKE) -E tar czf skred-$(VERSION)-maxed.tar.gz --format=gnutar skred-$(VERSION)-maxed; \
	else \
		$(CMAKE) -E chdir $(DIST_PLATFORM_ROOT) $(CMAKE) -E tar czf skred-$(VERSION)-maxed-linux-x86_64.tar.gz --format=gnutar skred-$(VERSION)-maxed; \
	fi
	@echo "==> Distribution archive ready at $(DIST_PLATFORM_ROOT)"

dist-api-linux-armhf: gen
	$(CMAKE) -B build_linux_armhf -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_C_COMPILER="arm-linux-gnueabihf-gcc"
	$(CMAKE) --build build_linux_armhf --target api api_shared mini-skred sk3 skred
	$(CMAKE) --install build_linux_armhf --prefix $(DIST_DIR)/linux-armhf/skred-$(VERSION)-maxed
	$(CMAKE) -E chdir $(DIST_DIR)/linux-armhf $(CMAKE) -E tar czf skred-$(VERSION)-maxed-linux-armhf.tar.gz --format=gnutar skred-$(VERSION)-maxed

dist-api-linux-aarch64: gen
	$(CMAKE) -B build_linux_aarch64 -S . -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_C_COMPILER="aarch64-linux-gnu-gcc"
	$(CMAKE) --build build_linux_aarch64 --target api api_shared mini-skred sk3 skred
	$(CMAKE) --install build_linux_aarch64 --prefix $(DIST_DIR)/linux-aarch64/skred-$(VERSION)-maxed
	$(CMAKE) -E chdir $(DIST_DIR)/linux-aarch64 $(CMAKE) -E tar czf skred-$(VERSION)-maxed-linux-aarch64.tar.gz --format=gnutar skred-$(VERSION)-maxed

windows-cross-libs: gen
	$(ZIG_ENV) $(CMAKE) --preset cross-windows-zig-maxed
	$(ZIG_ENV) $(CMAKE) --build --preset cross-windows-zig-maxed --target api api_shared mini-skred sk3 skred sk3-gui
	$(CMAKE) --install build_cross_windows_zig_maxed --prefix $(DIST_DIR)/windows-x86_64/skred-$(VERSION)-maxed

dist-api-windows: windows-cross-libs
	$(CMAKE) -E chdir $(DIST_DIR)/windows-x86_64 $(CMAKE) -E tar cf skred-$(VERSION)-maxed.zip \
		--format=zip skred-$(VERSION)-maxed

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
	cp -f build_wasm/skred_worklet_processor.js $(CURDIR)/../skode/app/editor/wasm/
	cp -f build_wasm/skred_worklet_engine.js $(CURDIR)/../skode/app/editor/wasm/
	cp -f build_wasm/skred_worklet_engine.wasm $(CURDIR)/../skode/app/editor/wasm/
	@echo "==> Running Skode tests..."
	@if command -v gleam >/dev/null 2>&1 && [ -d "$(CURDIR)/../skode" ]; then \
		(cd $(CURDIR)/../skode && gleam test && gleam test --target javascript); \
	fi
	@echo "==> skred and skode are synchronized!"

clean:
	rm -rf $(BUILD_DIR) build_wasm build_linux_armhf build_linux_aarch64 build_cross_windows_zig_maxed $(DIST_DIR) src/gen/test_schema_bounds tools/mksynth tools/mkopcodes *-history.txt .skred_history .sk3_history
