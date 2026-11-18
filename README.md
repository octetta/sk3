# skred (sk3) - Audio Engine & Skode Runtime

`skred` is a lightweight, high-performance polyphonic synthesizer, deterministic audio engine, and embedded Forth-inspired domain specific language (`skode`) for generative sound design and sequencing.

Version: `1.0.0`

## Features

- **Polyphonic Synthesizer Engine**: Multi-voice allocation with dynamic voice pooling and voice groups.
- **Rich DSP Primitives**: Dual-oscillator algorithms, phase distortion (CZ), frequency modulation (FM), resonant filters, exponential envelopes, polyblep anti-aliasing, and stereo delay lines.
- **Skode Command Language**: Compact, Forth-like reactive language with declarative grammar, pattern matching, guard expressions, and realtime opcode evaluation.
- **Dual Runtime Targets**:
  - **Native C**: High-performance multi-threaded CLI (`skred` / `mini-skred`) and static/shared libraries (`libapi.a`, `libapi.so`) with ALSA, CoreAudio, and MIDI support.
  - **WebAssembly**: Asynchronous, zero-dependency browser and headless Node.js engine via Emscripten.
- **Code-Generated Contract Architecture**: Synthesizer voice states and opcode dispatch tables are declaratively compiled from `.def` schema specifications via `mksynth` and `mkopcodes`, ensuring 100% boundary invariance and seamless Gleam interop.

## Quick Start

### Tooling (via mise)
Ensure dependencies are available (or activate via `mise`):
```bash
mise install
```

### Build Targets

```bash
make gen         # Compile declarative schemas and opcode tables
make test-schema # Verify schema boundary invariance
make all         # Build native libraries and skred CLI
make test        # Run full engine contract and state test suite (12/12)
make wasm        # Compile clean WebAssembly engine (build_wasm/skred_api.js/wasm)
make wasm-test   # Run headless Node.js verification test
```

## Repository Structure

- `schema/synth/`: Synthesizer engine state and allocation schema specifications (`.def`).
- `schema/opcodes/`: Opcode dictionary and dispatch schema specifications (`.def`).
- `doc/`: Architecture guides and generated reference manual (`REFERENCE.md`).
- `tools/`: Declarative code compilers (`mksynth.c`, `mkopcodes.c`).
- `src/`: Core C DSP, audio, and language runtime implementations.
- `src/gen/`: Generated C headers, dispatch tables, and equivalence harnesses.
- `waves/`: Essential wavetable sound recipes (`drums`, `dwg`, `esq1`).
- `wasm/`: Emscripten build tools and headless testing harness.
- `tests/`: Automated unit and contract test suites.
