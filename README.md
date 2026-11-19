# sk3 - Audio Engine & Skode Runtime

<p align="center">
  <img src="doc/assets/osko.png" alt="Osko the mascot" width="64" height="64" />
  <br>
  <em>Meet <strong>Osko</strong>, the official mascot of <code>sk3</code></em>
</p>

[![Release](https://img.shields.io/github/v/release/octetta/sk3?color=blue)](https://github.com/octetta/sk3/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Credits](https://img.shields.io/badge/credits-acknowledgements-purple.svg)](CREDITS.md)
[![Inspirations](https://img.shields.io/badge/history-inspirations-pink.svg)](doc/INSPIRATIONS.md)
[![Tests](https://img.shields.io/badge/tests-14%2F14%20passing-brightgreen.svg)]()

`sk3` (formerly skred) is a lightweight, high-performance polyphonic synthesizer, deterministic audio engine, and embedded Forth-inspired domain specific language (`skode`) for generative sound design and sequencing.

Version: `1.3.6`

## Features

- **Polyphonic Synthesizer Engine**: Multi-voice allocation with dynamic voice pooling and voice groups.
- **Bandlimited Wavetable Playback & Octave Mipmapping**: Automatic 8-level harmonic mipmap generation via in-place FFT / ksynth analysis, eliminating Nyquist aliasing foldover chirps in the upper register while unleashing 256-harmonic richness in the sub-bass, with $\pm 5\%$ boundary hysteresis and zero inner-loop CPU overhead.
- **Hardware LFSR Noise Generator**: Configurable Linear Feedback Shift Register noise engine on `w5` with classic vintage chip presets (MOS 6581/8580 SID, Atari POKEY 4/5/9/17-bit, NES 7/15-bit) and full runtime parameter control (`NT`).
- **Rich DSP Primitives**: Dual-oscillator algorithms, phase distortion (CZ), frequency modulation (FM), resonant filters with analog hyperbolic saturation drive (`jd`), combined filter modes (`L+B`, mode 6), exponential envelopes, sub-sample PolyBLEP anti-aliasing for hard sync, and stereo delay lines.
- **Skode Command Language**: Compact, Forth-like reactive language with declarative grammar, pattern matching, guard expressions, and realtime opcode evaluation.
- **Dual Runtime Targets**:
  - **Native C**: High-performance multi-threaded CLI (`sk3`, with compatibility aliases `skred` / `mini-skred`) and static/shared libraries (`libapi.a`, `libapi.so`) with ALSA, CoreAudio, and MIDI support.
  - **WebAssembly**: Asynchronous, zero-dependency browser and headless Node.js engine via Emscripten, featuring dedicated multi-threaded POSIX pthreads and stable WebAudio integration for glitch-free cross-browser real-time synthesis.
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
make all         # Build native libraries and sk3 CLI
make test        # Run full engine contract and state test suite (14/14)
make wasm        # Compile clean WebAssembly engine (build_wasm/skred_api.js/wasm)
make wasm-test   # Run headless Node.js verification test
```

## Repository Structure

- `schema/synth/`: Synthesizer engine state and allocation schema specifications (`.def`).
- `schema/opcodes/`: Opcode dictionary and dispatch schema specifications (`.def`).
- `doc/`: Architecture guides, generated reference manual (`REFERENCE.md`), evolutionary history (`HISTORY.md`), and historical lineage (`INSPIRATIONS.md`).
- `tools/`: Declarative code compilers (`mksynth.c`, `mkopcodes.c`).
- `src/`: Core C DSP, audio, and language runtime implementations.
- `src/gen/`: Generated C headers, dispatch tables, and equivalence harnesses.
- `waves/`: Essential wavetable sound recipes (`drums`, `dwg`, `esq1`).
- `wasm/`: Emscripten build tools and headless testing harness.
- `tests/`: Automated unit and contract test suites.

## License & Credits

`sk3` is released under the [MIT License](LICENSE) &copy; 2026 Joseph Ellis Stewart / Octetta.

For detailed third-party software acknowledgements, upstream libraries (miniaudio, miniz), and dogfooded components (k-synth, minimidio, uedit, ueval), please refer to [CREDITS.md](CREDITS.md).

For the project's evolution across previous iterations and prototypes, see [HISTORY.md](doc/HISTORY.md).

For the historical lineages and sonic pioneers that shaped `sk3` and the Skode language, see [INSPIRATIONS.md](doc/INSPIRATIONS.md).
