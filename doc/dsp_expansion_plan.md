# DSP Expansion Plan: Physical Modeling, Speech Synthesis & Sound Analysis

> [!NOTE]
> This document tracks the design roadmap, execution phases, and completed deliverables for the `sk3` DSP expansion. For the complete API reference and mathematical documentation, consult [`doc/DSP_TOOLKIT.md`](file:///home/stewartj/book/sk3/doc/DSP_TOOLKIT.md).

---

## 1. Executive Summary & Status

All Phase 1, Phase 1B, Phase 1C, and Sound Analysis capabilities have been fully implemented in [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h), verified with 16 automated test suites via `ctest`, and compiled into 8 standalone audio demonstrations via `make dsp-demos`.

```mermaid
flowchart TD
    subgraph Phase 1: Core Physical Modeling & Oscillators (DELIVERED)
        A1[Karplus-Strong Plucked Strings & Allpass Tuning]
        A2[Karplus-Strong Physical Drum Membrane]
        A3[Physical Waveguide Flute & Brass Models]
        A4[Band-Limited PolyBLEP Oscillators: Saw, Pulse/PWM, Tri]
    end

    subgraph Phase 1B: Formants & Historic Speech (DELIVERED)
        B1[5-Pole Parallel Formant Resonators with Lip Pre-emphasis]
        B2[~40 ARPAbet Phoneme Target Dictionary]
        B3[ASCII Speech DSL Compiler with Pitch Glides & Vibrato]
        B4[DECtalk & SAM Historic Voice Personas]
    end

    subgraph Phase 1C: Percussion & Modal Idiophones (DELIVERED)
        C1[Analog 808/909 Drums: Kick, Snare, Hats, Clap, Cowbell, Tom]
        C2[Euler-Bernoulli Modal Metallic Bells & Chimes]
    end

    subgraph Phase 1D: Sound Analysis & Transcription (DELIVERED)
        D1[Pure C Speech-to-Phoneme Transcriber: sk_speech_analyze_pcm]
        D2[10th-Order Levinson-Durbin LPC & Formant Peak Picking]
    end

    subgraph Phase 2: Engine & Network Control (DELIVERED)
        E1[Multiplexed Non-Blocking HTTP POST Control Endpoint on port 60440]
        E2[Per-Connection Isolated Skode Context with Keep-Alive]
    end

    subgraph Phase 3: Forth Words & Schema Integration (UPCOMING)
        F1[Expose physical pluck & flute/brass in Skode dictionary]
        F2[Expose speech DSL tokenizer as native Forth word]
        F3[Interactive 2D Formant Pad in web UI]
    end

    Phase 1 --> Phase 1B --> Phase 1C --> Phase 1D --> Phase 2 --> Phase 3
```

---

## 2. Implementation Deliverables Matrix

| Component | Target Architecture | Implementation File | Status | Verification Target |
| :--- | :--- | :--- | :--- | :--- |
| **Plucked String** | Extended Karplus-Strong with allpass fractional delay and pick comb filter | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_karplus_t`) | **Complete** | `pluck_demo.wav` |
| **Physical Drum** | 2D membrane pitch-drop waveguide with inverting loop filter | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_karplus_strike_drum`) | **Complete** | `percussion_demo.wav` |
| **Physical Flute** | McIntyre-Schumacher-Woodhouse non-linear jet-drive waveguide | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_flute_t`) | **Complete** | `wind_brass_demo.wav` |
| **Physical Brass** | Spring-mass lip reed, Bernoulli flow, and brassiness shock waves | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_brass_t`) | **Complete** | `wind_brass_demo.wav` |
| **PolyBLEP Waveforms** | 2nd-order polynomial bandlimited step (Saw, Pulse/PWM, Triangle) | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_polyblep_osc_t`) | **Complete** | `polyblep_demo.wav` |
| **Formant Resonators** | 5-pole parallel bandpass bank ($F_1 \dots F_5$) with lip radiation pre-emphasis | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_formant_bank_t`) | **Complete** | `vowel_demo.wav` |
| **Speech DSL** | ASCII phoneme tokenizer with pitch inflection `[PHO:ms@f0>f1~]` | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_phoneme_compile_dsl`) | **Complete** | `phoneme_demo.wav`, `speech_demo.wav` |
| **Historic Voices** | DECtalk (Paul, Betty, Harry), SAM C64, Kit the Kid, Robot Monotone | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_speech_voice_*`) | **Complete** | `voices_demo.wav` |
| **Analog Drums** | 808/909 Kick, Snare, Hi-Hats, Clap, Cowbell, and Tom | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_drum_*`) | **Complete** | `percussion_demo.wav` |
| **Modal Bells** | 6-mode Euler-Bernoulli inharmonic metallic idiophones (5 presets) | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_drum_bell_t`) | **Complete** | `percussion_demo.wav` |
| **LPC Speech Analysis**| 10th-order Levinson-Durbin all-pole LPC & formant peak picking in pure C | [`src/sk_dsp.h`](file:///home/stewartj/book/sk3/src/sk_dsp.h) (`sk_speech_analyze_pcm`) | **Complete** | `tests/dsp_math_tests.c` |
| **HTTP POST Endpoint** | Multiplexed non-blocking POST control server on port 60440 | [`src/http.c`](file:///home/stewartj/book/sk3/src/http.c), [`src/http.h`](file:///home/stewartj/book/sk3/src/http.h) | **Complete** | `tests/http_tests.c` |

---

## 3. Acoustic Modeling Details

### A. Extended Karplus-Strong Physical String
* **Allpass Fractional Delay**: Solves pitch-quantization without introducing artificial high-frequency damping ($|A(z)| = 1.0$ across all frequencies).
* **Pick Position Comb**: Feedforward comb $(x[n] - x[n - D_{\text{pick}}])$ creates harmonic notches corresponding to picking close to the bridge (bright) vs. center (warm).

### B. Physical Flute & Brass
* **Flute**: Models turbulent air jet crossing the labium window. Non-linear cubic saturation creates alternating edge-tone vortices, coupled back into a reflection bore.
* **Brass**: Simulates player's lips acting as an outward-striking spring-mass valve. High-pressure air from mouth creates non-linear shock waves steepening into brassy buzz.

### C. Anti-Aliased PolyBLEP Oscillators
* Residual step correction eliminates Nyquist foldover distortion without expensive 4x/8x oversampling.
* Supports dynamic pulse-width modulation (PWM) from 1% to 99% with sub-sample transient accuracy.

### D. Historic Speech & Formant Synthesis
* **Klatt-Style Resonator Bank**: Constant 0 dB peak gain bandpass filters prevent gain explosion when formants sweep or cluster.
* **Lip Radiation Tilt**: First-difference filter ($y[n] = x[n] - 0.72x[n-1]$) adds natural +6 dB/octave high-frequency acoustic presence.
* **Stop Plosive Transients**: Instantaneous pressure burst injection replicates alveolar, velar, and bilabial release pops (P, T, K, B, D, G).
* **DECtalk / SAM Voice Personas**: Replicates historic speech synthesis systems with parameterized vocal tract lengths and fundamental pitch envelopes.

### E. Modal Metallic Bells
* Unlike 2D drums that drop in pitch on impact, struck metallic solids possess structural stiffness governed by Euler-Bernoulli beam and shell mechanics:
  $$f_n = f_0 \cdot \mu_n$$
* Presets include bronze church bells, orchestral tubular chimes, vibraphone bars, Tibetan singing bowls (with beating modal doublets), and Latin agogo bells.

### F. Pure C Sound Analysis & Speech-to-Phoneme
* Independent of Python, external neural network weights, or foreign C libraries.
* Employs 3:1 integer decimation (to 16 kHz), 25 ms Hamming windows, zero-crossing rate calculation, and 10th-order Levinson-Durbin autocorrelation LPC recursion.
* Extracts resonant $F_1, F_2, F_3$ spectral peaks and maps them to standard ARPAbet phonetic centroids.

---

## 4. Phase 3 Roadmap: Skode Language & Engine Integration

With all math, physics, and speech primitives fully implemented and tested in `sk_dsp.h`, Phase 3 integrates these engines into the real-time `sk3` synthesizer and Forth-like `skode` language:

1. **Physical Modeling Voice Modes**:
   - `w6` physical string voice mode (`pluck`).
   - `w7` physical wind/brass voice mode (`wind`, `brass`).
2. **Formant Resonator Filter Modes**:
   - Expand `sk3` filter types to include 5-pole vocal tract formant filtering (`formant`).
3. **Native Speech Opcode**:
   - Forth word `say` to evaluate inline speech DSL tokens:
     ```forth
     "[HH:70 EH1:110 L:80 OW1:200]" say
     ```
4. **Interactive Web UI**:
   - 2D vowel plane pad $(X, Y)$ and realtime phoneme sequencer in `skred-web`.
