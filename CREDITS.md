# Credits & Acknowledgements

`sk3` (v1.0.0) stands on the shoulders of remarkable open-source projects, seminal computer music research, and tightly integrated dogfooded libraries created by Joseph Stewart / Octetta.

For the philosophical, sonic, and architectural history behind `sk3`, see [INSPIRATIONS.md](doc/INSPIRATIONS.md).

---

## Creator & Architecture

- **Joseph Ellis Stewart ([Octetta](https://github.com/octetta))**
  - Synthesizer core architecture, polyphony engine, dynamic voice allocation, and DSP synthesis voice operators.
  - Creator and designer of the **Skode** domain-specific language, reactive command parser, real-time opcode dispatch, and declarative code-generation toolchain (`mksynth`, `mkopcodes`).
  - WebAssembly runtime bridge, multi-threaded control dispatchers, and virtual file system.

---

## First-Party Dogfooded Libraries

The following companion projects developed by **Joseph Stewart / Octetta** are vendored or embedded directly into `sk3`:

- **[k-synth](https://github.com/octetta/k-synth)** (`src/vendor/ksynth/`)
  - **Version / Revision**: Git commit `307a651038b9ca115340e697352134475e77345b`
  - Fast, array-oriented DSP vector language, expression evaluator, and bandlimited wavetable oscillator bank.
  - Heavily inspired by the **[ksimple](https://github.com/kparc/ksimple)** lineage by **Arthur Whitney**.
  - Licensed under the MIT License.
- **[minimidio](https://github.com/octetta/minimidio)** (`src/vendor/minimidio/`)
  - **Version / Revision**: Git commit `0d1f33a24ca1aa9b8d22727e42cdda057570992a`
  - Lightweight single-header cross-platform MIDI input and output interface supporting ALSA, CoreMIDI, Windows MM, and Web MIDI.
  - Licensed under the MIT License.
- **[uedit](https://github.com/octetta/uedit)** (`src/vendor/uedit/`)
  - **Version / Revision**: Git commit `4e618f588de232d86f64d7c73d7082ede52d9cac`
  - Zero-dependency ANSI/VT100 interactive terminal line editor providing readline-style command editing, multiline support, and command history persistence.
  - Licensed under the MIT License.
- **[ueval](https://github.com/octetta/ueval)**
  - **Version / Revision**: Git commit `7860...`
  - Micro arithmetic evaluator and expression parsing patterns that informed the compact numeric token dispatch and realtime evaluators in the Skode runtime.
  - Licensed under the MIT License.

---

## Third-Party Open Source Software

`sk3` gratefully incorporates and builds upon the following open-source software libraries:

### 1. [miniaudio](https://github.com/mackron/miniaudio)
- **Version**: `v0.11.25` (2026-03-04)
- **Author**: David Reid ([mackron@gmail.com](mailto:mackron@gmail.com))
- **Website**: [miniaud.io](https://miniaud.io)
- **Role**: Cross-platform low-latency audio hardware playback, capture, and device enumeration supporting Linux (ALSA/PulseAudio), macOS (CoreAudio), Windows (WASAPI/DirectSound), and WebAssembly (Web Audio API via Emscripten).
- **License**: Public Domain (Unlicense) / MIT-0.

### 2. [miniz](https://github.com/richgel99/miniz)
- **Version**: `v2.1.0`
- **Author**: Rich Geldreich (Tenacious Software LLC / Valve Software) & contributors.
- **Role**: High-speed, public-domain Deflate/Inflate and ZIP archive reader/writer powering `sk3`'s in-memory Virtual File System (`src/exp-vfs/`), allowing zero-copy runtime mounting and streaming of asset bundles (`.zip`).
- **License**: Public Domain (Unlicense) / MIT License.

### 3. [ksimple](https://github.com/kparc/ksimple)
- **Author**: Arthur Whitney / [kparc](https://github.com/kparc).
- **Role**: Minimalist, high-performance K array language interpreter lineage that inspired `k-synth`'s vector math semantics and whole-buffer signal processing.
- **Website**: [kparc.io](https://kparc.io) / [Kx Systems](https://kx.com)

---

## Language & Computing Lineages

- **Charles H. "Chuck" Moore ([Forth](https://www.forth.com/about-forth/))**:
  - Pioneer of Forth, stack architectures, and radical programming simplicity.
  - Skode's dictionary word execution, postfix argument model, and interactive live-coding ergonomics trace their lineage directly to Chuck Moore's philosophy: *"Keep it simple. As simple as you can. Do not solve problems you don't have."*

---

## Musical DSP & Acoustic Modeling Foundations

- **PolyBLEP Bandlimited Oscillators**: Antialiased polynomial bandlimited step (BLEP) formulations for bandlimited hard-syncable sawtooth, pulse/PWM, and triangle oscillators with minimal CPU overhead.
- **Casio CZ Phase Distortion (PD)**: Phase distortion algorithms and resonance window functions inspired by the classic Casio CZ-101 / CZ-1 synthesis engines.
- **Frequency Modulation (FM)**: Multi-operator algorithmic frequency modulation with feedback loops.
- **State Variable Filters (SVF)**: Resonant digital state-variable filter topologies providing continuous lowpass, bandpass, highpass, and notch outputs.
- **Digital Waveguide (DWG)**: Physical acoustic modeling algorithms for strings, acoustic piano, reeds, and brass instruments.
- **Ensoniq ESQ-1 Wavetable Data**: Classic 8-bit digital wavetable single-cycle waveforms and multi-sample recipes.
