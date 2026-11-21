# DSP Expansion Plan: Physical Modeling & Full Phoneme/Formant Synthesis

## 1. Architectural Perspective & Evaluation

### A. String Pluck: Extended Karplus-Strong (KS)
* **The Concept**: Digital waveguide physical modeling where a burst of energy circulates in a feedback delay loop with a loss filter.
* **Why it fits `sk3` / `k-synth`**:
  - Extremely cheap computationally (a circular buffer read/write, one multiplication, one addition per sample).
  - High dynamic and acoustic range: changes naturally depending on pluck velocity, pick position, and string damping.
  - Complements the existing subtractive/wavetable architecture without requiring heavy new memory structures.
* **Implemented in Phase 1**:
  - **Allpass Fractional Delay Tuning**: Maintains constant loop magnitude $|H(e^{j\omega})| = 1.0$, allowing crisp pitch tuning across octaves without artificial high-frequency damping.
  - **Pick Position Comb**: Feedforward comb $(x[n] - x[n - D_{\text{pick}}])$ creates harmonic notches corresponding to plucking near the bridge (bright/thin) vs. center (warm/round).

---

### B. Vocal Tract: Vowels & Full Phoneme Generation
* **The Concept**: Source-filter acoustic model of speech. An excitation source (glottal pulse train for voiced sounds, shaped noise for unvoiced fricatives, or silence-and-burst for plosives) drives a bank of 4 to 5 tuned bandpass formant resonators ($F_1 \dots F_5$) and a parallel frication noise pathway.
* **The 4 Phonetic Acoustic Classes**:
  1. **Voiced Sonorants (Vowels & Glides)**: `/a/`, `/e/`, `/i/`, `/o/`, `/u/`, `/l/`, `/r/`, `/w/`, `/j/`.
     - Driven by Rosenberg glottal pulse ($g(t) = 3t^2 - 2t^3$) with mild pitch jitter and vibrato.
     - Resonated through parallel constant-0dB 2-pole bandpass filters ($F_1, F_2, F_3, F_4$).
  2. **Fricatives & Sibilants**: `/s/`, `/ʃ/` ("sh"), `/f/`, `/θ/` ("th"), `/z/`, `/ʒ/`, `/v/`.
     - High-frequency shaped noise injected into the vocal tract or mixed with voice:
       - `/s/`: Bandpass noise centered at **5.0 kHz – 8.0 kHz**.
       - `/ʃ/`: Turbulent noise centered at **2.5 kHz – 4.5 kHz**.
       - Voiced fricatives (`/z/`, `/v/`): Glottal pulse and noise simultaneously active.
  3. **Plosives / Stops**: `/p/`, `/t/`, `/k/` (unvoiced), `/b/`, `/d/`, `/g/` (voiced).
     - 3-phase state machine:
       $$\text{Voice bar / closure} \longrightarrow \text{Complete silence (20–60ms)} \longrightarrow \text{High-pressure release burst (5–15ms)}$$
  4. **Nasals**: `/m/`, `/n/`, `/ŋ/` ("ng").
     - Closed oral cavity branches off open nasal cavity, creating an anti-formant (spectral zero / notch) between 800 Hz and 1800 Hz, with a prominent low "nasal murmur" at ~250 Hz.

---

## 2. Phased Implementation Roadmap

```mermaid
flowchart TD
    subgraph Phase 1: Core DSP Math (COMPLETED)
        A1[sk_dsp.h: Karplus-Strong string engine with allpass tuning & pick comb]
        A2[sk_dsp.h: Rosenberg glottal generator & 4-pole Formant Bank]
        A3[dsp_math_tests.c: 15 unit tests passing 100%]
        A4[render_dsp_demo: pluck_demo.wav and vowel_demo.wav generated]
    end

    subgraph Phase 1B: Full Phoneme Primitives & Dictionary (Token Safe)
        B1[sk_dsp.h: Add ARPAbet ~40-phoneme acoustic parameter table]
        B2[sk_dsp.h: Add Fricative/Stop noise shaper & Plosive burst generator]
        B3[sk_dsp.h: Add Coarticulation parameter smoother]
        B4[tools/render_dsp_demo: Render spoken words demo e.g. 'SKRED']
    end

    subgraph Phase 2: Interactive Playground in k-synth
        C1[Expose Karplus-Strong & Formant controls in k-synth]
        C2[UI 2D Vowel XY Pad & Phoneme String Trigger]
    end

    subgraph Phase 3: sk3 Engine & Skode Forth Integration (Next Week)
        D1[Voice mode: Karplus-Strong physical pluck]
        D2[Voice filter mode: Formant bank resonator]
        D3[Skode Forth words: e.g. [H EH L OW] say or v0 form x y]
        D4[Cross-repo release: sk3, skode, skred-web, mod/sid clients]
    end

    Phase 1 --> Phase 1B
    Phase 1B --> Phase 2
    Phase 2 --> Phase 3
```

---

## 3. Detailed Component Specifications

### Component 1: `sk_karplus_t` (Plucked String Engine) — *Completed*
* **Location**: [sk3/src/sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h#L834-L968)
* **Features**: Self-contained circular buffer, 1st-order allpass fractional delay tuning, dual-parameter pluck (pick position comb + strike brightness lowpass), 1-pole loop damping, DC blocker.

---

### Component 2: `sk_formant_bank_t` (Vocal Resonator Engine) — *Completed*
* **Location**: [sk3/src/sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h#L970-L1136)
* **Features**: Rosenberg polynomial glottal wave generator ($g(t) = 3t^2 - 2t^3$), 4-pole parallel constant 0 dB peak bandpass resonators, canonical vowel selector (`/a/`, `/e/`, `/i/`, `/o/`, `/u/`), continuous 2D $(X, Y)$ vowel morphing plane.

---

### Component 3: `sk_phoneme_t` (Full Phoneme Dictionary & Engine)
* **Location**: [sk3/src/sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h)
* **Phoneme Acoustic Definition**:
  ```c
  typedef struct {
      char code[4];          /* ARPAbet code: "AA", "S", "T", "SH", "M", etc. */
      uint8_t flags;         /* Bit 0: voiced, Bit 1: nasal, Bit 2: plosive, Bit 3: fricative */
      float duration_ms;     /* Standard steady-state duration in ms */
      float voice_gain;      /* Glottal pulse level (0.0 to 1.0) */
      float noise_gain;      /* Aspiration/frication noise level (0.0 to 1.0) */
      float noise_freq;      /* Frication filter center frequency in Hz */
      float noise_bw;        /* Frication filter bandwidth in Hz */
      float f1, f2, f3, f4;  /* Formant target frequencies in Hz */
      float b1, b2, b3, b4;  /* Formant bandwidths in Hz */
      float g1, g2, g3, g4;  /* Formant relative gains */
  } sk_phoneme_def_t;
  ```

* **ARPAbet Subset (~40 Standard Phonemes)**:
  - **Vowels**: `AA` (father), `AE` (cat), `AH` (but), `AO` (dog), `EH` (bed), `ER` (bird), `EY` (say), `IH` (sit), `IY` (see), `OW` (go), `UH` (book), `UW` (too).
  - **Fricatives**: `S` (sea), `SH` (she), `Z` (zoo), `ZH` (measure), `F` (fan), `V` (van), `TH` (thin), `DH` (this).
  - **Stops/Plosives**: `P` (pat), `B` (bat), `T` (tap), `D` (dam), `K` (cat), `G` (gap).
  - **Nasals**: `M` (mom), `N` (no), `NG` (sing).
  - **Glides & Liquids**: `L` (lay), `R` (red), `W` (way), `Y` (yes), `HH` (hay).

* **Coarticulation Transition Filter**:
  A parameter slew rate limiter:
  $$P_{k}[n] = P_{k}[n-1] + \alpha \cdot \left(\text{Target}_{k} - P_{k}[n-1]\right)$$
  Where $\alpha \approx 0.005$ to $0.02$, ensuring formant frequencies sweep smoothly between phonemes to sound like human vocal tract movement instead of abrupt clicks.

---

### Component 4: Skode Forth Syntax Design (Phase 3)
* **Pluck Command**:
  ```forth
  v0 440 0.8 0.2 pluck    \ voice 0, 440 Hz, vel 0.8, pick pos 0.2
  ```
* **Vowel / Formant Filter Command**:
  ```forth
  v0 vowel A              \ set voice 0 formant filter to canonical 'A'
  v0 0.5 -0.2 form_xy     \ set voice 0 to continuous 2D vowel plane coordinate
  ```
* **Phoneme Speech Command**:
  ```forth
  [S K R EH D] phoneme    \ speak "skred" using ARPAbet phoneme sequence
  ```
