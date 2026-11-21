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
[![Tests](https://img.shields.io/badge/tests-17%2F17%20passing-brightgreen.svg)]()

`sk3` (formerly skred) is a lightweight, high-performance polyphonic synthesizer, deterministic audio engine, and embedded Forth-inspired domain specific language (`skode`) for generative sound design and sequencing.

Version: `1.4.0`

## Features

- **Polyphonic Synthesizer Engine**: Multi-voice allocation with dynamic voice pooling and voice groups.
- **Embedded DSP & Physical Modeling Toolkit**: Comprehensive single-file C DSP library (`sk_dsp.h`) providing physical plucked strings, acoustic flute and brass waveguides, anti-aliased PolyBLEP oscillators, 5-pole formant speech synthesis with historic DECtalk/SAM personas, analog 808/909 percussion, Euler-Bernoulli modal bells, and pure C Levinson-Durbin LPC speech-to-phoneme analysis (see [DSP Toolkit Reference](doc/DSP_TOOLKIT.md) and [DSP Expansion Plan](doc/dsp_expansion_plan.md)).
- **Polymorphic TCP, HTTP & WebSocket Control Server**: Zero-dependency polymorphic TCP endpoint on port 60440 (`-T60440`, alias `-H60440`) featuring single-threaded non-blocking `select()` multiplexing, automatic protocol sniffing (Raw ASCII stream, HTTP/1.1 REST, and RFC 6455 WebSocket), per-connection isolated Forth contexts, persistent `keep-alive` and WebSocket frame streaming, in-engine `/s` diagnostics, and full CORS headers with Private Network Access for browser XY pads and web apps (see [TCP/HTTP/WebSocket Reference](doc/HTTP_API.md)).
- **Bandlimited Wavetable Playback & Octave Mipmapping**: Automatic 8-level harmonic mipmap generation via in-place FFT / ksynth analysis, eliminating Nyquist aliasing foldover chirps in the upper register while unleashing 256-harmonic richness in the sub-bass, with $\pm 5\%$ boundary hysteresis and zero inner-loop CPU overhead.
- **Hardware LFSR Noise Generator**: Configurable Linear Feedback Shift Register noise engine on `w5` with classic vintage chip presets (MOS 6581/8580 SID, Atari POKEY 4/5/9/17-bit, NES 7/15-bit) and full runtime parameter control (`NT`).
- **Rich DSP Primitives**: Dual-oscillator algorithms, phase distortion (CZ), frequency modulation (FM), resonant filters with analog hyperbolic saturation drive (`jd`), combined filter modes (`L+B`, mode 6), exponential envelopes, sub-sample PolyBLEP anti-aliasing for hard sync, and stereo delay lines.
- **Skode Command Language**: Compact, Forth-like reactive language with declarative grammar, pattern matching, guard expressions, and realtime opcode evaluation.
- **Dual Runtime Targets**:
  - **Native C**: High-performance multi-threaded CLI (`sk3`, with compatibility aliases `skred` / `mini-skred`) and static/shared libraries (`libapi.a`, `libapi.so`) with ALSA, CoreAudio, and MIDI support.
  - **WebAssembly**: Asynchronous, zero-dependency browser and headless Node.js engine via Emscripten, featuring dedicated multi-threaded POSIX pthreads and stable WebAudio integration for glitch-free cross-browser real-time synthesis.
- **Code-Generated Contract Architecture**: Synthesizer voice states and opcode dispatch tables are declaratively compiled from `.def` schema specifications via `mksynth` and `mkopcodes`, ensuring 100% boundary invariance and seamless Gleam interop.

## Client & Ecosystem Integration Matrix

`sk3` listens on a single polymorphic TCP port (**`60440`** by default, configured via `-T<port>`) that automatically detects the client's protocol and escalates from Raw ASCII line streaming to HTTP/1.1 REST or RFC 6455 WebSockets:

| Community & Sound Client | Connection Type | Protocol | How It Connects & What It Unlocks |
| :--- | :--- | :--- | :--- |
| **Chiptune & Tracker Replay**<br>*(SID-to-Skode, MOD-to-Skode)* | **BOP (Binary) / UDP** | 40-byte Binary Opcode Protocol | 50/60 Hz raster-tick dispatching of pitch, PW, and gate updates with zero string allocations, zero GC pressure, and microsecond-level timing accuracy. |
| **Urbit Ecosystem**<br>*(Mars, Hoon Gall agents)* | **WebSocket / HTTP** | RFC 6455 Binary / ASCII | Gall agents pack Hoon `@ux` binary opcode structs directly to `%iris` / `%eyre` WebSocket channels without costly atom-to-tape string parsing. |
| **SynthDIY & Hardware Hackers**<br>*(ESP32, Teensy, Pico W, Arduino)* | **Raw TCP / UDP / BOP** | ASCII or 40-byte Binary | `client.println("v0 K1200 Q3.5")` or binary UDP struct dispatch; physical potentiometers stream straight to DSP voices with zero library overhead. |
| **Livecoding & Algorave**<br>*(TidalCycles, FoxDot, Orca, Sonic Pi)* | **Raw TCP / Pipe** | ASCII Lines | `nc localhost 60440`<br>Instant terminal REPL with Unix pipes; per-connection isolated Forth namespaces allow multi-user jam sessions on the same engine without macro collision. |
| **Browser Web Apps & Strudel**<br>*(Strudel.cc, Canvas, AudioWorklet)* | **WebSocket / WASM** | RFC 6455 (Text & Binary) | `new WebSocket("ws://localhost:60440/")` or `SharedArrayBuffer` AudioWorklet ringbuffer; high-rate XY controller sweeps execute with zero reverse packet backpressure. |
| **Visuals & Generative Art**<br>*(TouchDesigner, Hydra, Max/MSP, p5.js)* | **WebSocket / TCP** | RFC 6455 / Raw TCP | Native TouchDesigner TCP/WebSocket DATs, Max/MSP `[tcpsocket]`, or Hydra web canvas modulating filter cutoffs (`K`) and resonance (`Q`) at 60–120 Hz with sub-millisecond response. |
| **Visual Audio Patching**<br>*(PureData, SuperCollider)* | **Raw TCP / UDP** | ASCII / Binary BOP | Pd `[netsend -u -b]` or SuperCollider `NetAddr.sendRaw`<br>Native socket streaming without external VST/OSC bridge daemons or JACK routing ceremony. |
| **BEAM Ecosystem**<br>*(Gleam, Elixir, Erlang, LFE)* | **Raw TCP / UDP** | `:gen_tcp` / `:gen_udp` | Elixir binary pattern matching (`<<opcode::16, voice::16, argc::8, ...>>`); GenStage pipelines stream binary frames to `sk3` at line rate. |
| **Game Dev & Creative Engines**<br>*(Godot, LÖVE 2D, Bevy / Rust)* | **TCP / WebSocket / C** | StreamPeerTCP / C API | Procedural sound generation driven directly from game scripts or C ABI without compiling native audio driver plugins. |
| **AI & Computational Music**<br>*(Jupyter, Python, Julia, R)* | **HTTP REST / WS** | POST / WebSocket | `urllib.request` / `requests.post`<br>Audition algorithmic compositions directly from notebook cells without audio device kernel locking. |
| **Glitch, Demoscene & Trackers**<br>*(Shell pipes, Renoise Lua, byte-mangling)* | **Raw TCP / Shell** | Fuzzed ASCII / Text | Uncrashable parser with graceful `# SKODE_ERROR` reporting; vintage hardware LFSR noise models (`w5` MOS 6581 SID, POKEY, NES) and analog saturation drive (`jd`). |

See the [TCP, HTTP & WebSocket Reference](doc/HTTP_API.md) and the [Binary Opcode Protocol (BOP) Specification](doc/BINARY_OPCODE_PROTOCOL.md) for complete standalone code examples, including interactive browser XY controller pads and chiptune replay dispatchers.

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
make test        # Run full engine contract and state test suite (17/17)
make dsp-demos   # Render all 8 DSP physical modeling & speech WAV demos
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
