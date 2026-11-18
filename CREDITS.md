# Credits & Acknowledgements

`sk3` stands on the shoulders of remarkable open-source projects, seminal computer music research, and tightly integrated dogfooded libraries created by Joseph Stewart / Octetta.

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
  - Fast, array-oriented DSP vector language, expression evaluator, and bandlimited wavetable oscillator bank.
  - Licensed under the MIT License.
- **[minimidio](https://github.com/octetta/minimidio)** (`src/vendor/minimidio/`)
  - Lightweight single-header cross-platform MIDI input and output interface supporting ALSA, CoreMIDI, Windows MM, and Web MIDI.
  - Licensed under the MIT License.
- **[uedit](https://github.com/octetta/uedit)** (`src/vendor/uedit/`)
  - Zero-dependency ANSI/VT100 interactive terminal line editor providing readline-style command editing, multiline support, and command history persistence.
  - Licensed under the MIT License.
- **[ueval](https://github.com/octetta/ueval)**
  - Micro arithmetic evaluator and expression parsing patterns that informed the compact numeric token dispatch and realtime evaluators in the Skode runtime.
  - Licensed under the MIT License.

---

## Third-Party Open Source Software

`sk3` gratefully incorporates and builds upon the following open-source software libraries:

### 1. [miniaudio](https://github.com/mackron/miniaudio)
- **Author**: David Reid ([mackron@gmail.com](mailto:mackron@gmail.com))
- **Website**: [miniaud.io](https://miniaud.io)
- **Role**: Cross-platform low-latency audio hardware playback, capture, and device enumeration supporting Linux (ALSA/PulseAudio), macOS (CoreAudio), Windows (WASAPI/DirectSound), and WebAssembly (Web Audio API via Emscripten).
- **License**: Public Domain (Unlicense) / MIT-0.

### 2. [miniz](https://github.com/richgel99/miniz)
- **Author**: Rich Geldreich (Tenacious Software LLC / Valve Software) & contributors.
- **Role**: High-speed, public-domain Deflate/Inflate and ZIP archive reader/writer powering `sk3`'s in-memory Virtual File System (`src/exp-vfs/`), allowing zero-copy runtime mounting and streaming of asset bundles (`.zip`).
- **License**: Public Domain (Unlicense) / MIT License.

---

## Musical DSP & Acoustic Modeling Foundations

- **PolyBLEP Bandlimited Oscillators**: Antialiased polynomial bandlimited step (BLEP) formulations for bandlimited hard-syncable sawtooth, pulse/PWM, and triangle oscillators with minimal CPU overhead.
- **Casio CZ Phase Distortion (PD)**: Phase distortion algorithms and resonance window functions inspired by the classic Casio CZ-101 / CZ-1 synthesis engines.
- **Frequency Modulation (FM)**: Multi-operator algorithmic frequency modulation with feedback loops.
- **State Variable Filters (SVF)**: Resonant digital state-variable filter topologies providing continuous lowpass, bandpass, highpass, and notch outputs.
- **Digital Waveguide (DWG) & Karplus-Strong**: Physical modeling algorithms for strings, acoustic piano, reeds, and brass instruments.
- **Ensoniq ESQ-1 Wavetable Data**: Classic 8-bit digital wavetable single-cycle waveforms and multi-sample recipes.
