# DSP Expansion Plan: Physical Modeling & Formant/Phoneme Synthesis

## 1. Architectural Perspective & Evaluation

### A. String Pluck: Extended Karplus-Strong (KS)
* **The Concept**: Digital waveguide physical modeling where a burst of energy is circulated in a feedback delay loop with a loss filter.
* **Why it fits `sk3` / `k-synth`**:
  - Extremely cheap computationally (a circular buffer read/write, one multiplication, one addition per sample).
  - High dynamic and acoustic range: changes naturally depending on pluck velocity, pick position, and string damping.
  - Complements the existing subtractive/wavetable architecture without requiring heavy new memory structures.
* **Key Technical Decisions**:
  - **Fractional Delay Tuning**: Standard linear interpolation creates artificial damping that shortens high-pitch string sustain. An **allpass 1st-order interpolator** maintains constant loop magnitude $|H(e^{j\omega})| = 1.0$, allowing crisp pitch tuning across octaves.
  - **Pick Position Comb**: A simple feedforward tap $(x[n] - x[n - D_{\text{pick}}])$ creates harmonic notches corresponding to plucking near the bridge (bright/thin) vs. center (warm/round).

---

### B. Phonemes & Formants: Vocal Tract Resonator Bank
* **The Concept**: Source-filter acoustic model of speech. An excitation source (glottal pulse train for voiced sounds, shaped noise for unvoiced fricatives) drives a bank of 3 to 5 tuned bandpass formant resonators ($F_1 \dots F_5$).
* **Why it fits `sk3` / `k-synth`**:
  - Existing wavetable/subtractive synths struggle to make convincing vocal sounds without dedicated formant filtering.
  - `sk_dsp.h` already has fast 2-pole resonators (`sk_resonant2p_t`) and state-variable filters (`sk_svf_t`).
* **Key Technical Decisions**:
  - **Parallel vs. Cascade Filter Bank**:
    - *Cascade*: Natural vocal tract physics for non-nasal vowels, but harder to tune gains independently.
    - *Parallel* (Recommended): Each formant $F_k$ has independent frequency, bandwidth, and gain. Much easier to modulate, morph, and prevent clipping.
  - **2D Vowel Map**: Rather than discrete jumps between phonemes, implement a continuous 2D coordinate space $(X, Y)$ mapping to canonical vowels (e.g. $[A, E, I, O, U]$), allowing fluid vowel morphing via LFOs or pitch tracking.
  - **Glottal Exciter**: Pure impulse trains buzz too harshly; a Rosenberg glottal model (polynomial rise and exponential drop) gives warm, human vocal character.

---

## 2. Phased Implementation Roadmap

```mermaid
flowchart TD
    subgraph Phase 1: Pure DSP Math (Safe for 11% Budget)
        A1[sk_dsp.h: Add allpass fractional delay & Karplus-Strong primitive]
        A2[sk_dsp.h: Add Rosenberg glottal generator & Formant Bank]
        A3[dsp_math_tests.c: Unit tests & stability boundaries]
    end

    subgraph Phase 2: Interactive Validation in k-synth
        B1[Integrate DSP primitives into k-synth audio thread]
        B2[UI controls: Pluck position, damping, 2D vowel morph XY pad]
    end

    subgraph Phase 3: sk3 Engine Integration (Defer to Next Week)
        C1[Add KS oscillator mode to synth-voice.c]
        C2[Add Formant Filter mode to voice filter architecture]
        C3[Expose Skode Forth opcodes for pluck & phoneme morphing]
        C4[Multi-repo release: sk3, skode, skred-web, mod/sid clients]
    end

    Phase 1 --> Phase 2
    Phase 2 --> Phase 3
```

---

## 3. Detailed Component Specifications

### Component 1: `sk_karplus_t` (Plucked String Engine)
* **Location**: [sk3/src/sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h)
* **State Struct**:
  ```c
  typedef struct {
      float *buffer;
      int size;
      int write_pos;
      float damping;       // Loop 1-pole lowpass coefficient (0.0 to 1.0)
      float feedback;      // Decay factor (0.95 to 0.999)
      float delay_samples; // Target loop length (sample_rate / freq)
      float allpass_state; // Fractional allpass memory
      float allpass_coeff; // Allpass coefficient for sub-sample tuning
  } sk_karplus_t;
  ```
* **API**:
  - `sk_karplus_init(sk_karplus_t *ks, float *buf, int max_size, float sample_rate)`
  - `sk_karplus_pluck(sk_karplus_t *ks, float freq, float velocity, float pick_pos)`
  - `sk_karplus_step(sk_karplus_t *ks) -> float`

---

### Component 2: `sk_formant_bank_t` (Vocal Resonator Engine)
* **Location**: [sk3/src/sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h)
* **Vowel Table**:
  Standard Peterson & Barney / Fant formant frequencies (Hz) for male/female/neutral vocal tracts:
  | Vowel | IPA | $F_1$ (Hz) | $F_2$ (Hz) | $F_3$ (Hz) | Description |
  | :--- | :--- | :--- | :--- | :--- | :--- |
  | **A** | `/ɑ/` | 730 | 1090 | 2440 | "father" |
  | **E** | `/e/` | 530 | 1840 | 2480 | "bed" |
  | **I** | `/i/` | 270 | 2290 | 3010 | "meet" |
  | **O** | `/o/` | 500 | 850 | 2250 | "boat" |
  | **U** | `/u/` | 300 | 870 | 2240 | "boot" |
* **API**:
  - `sk_formant_bank_init(sk_formant_bank_t *fb, float sample_rate)`
  - `sk_formant_set_vowel(sk_formant_bank_t *fb, int vowel_id, float blend)`
  - `sk_formant_set_xy(sk_formant_bank_t *fb, float x, float y)` (Continuous vowel map)
  - `sk_formant_step(sk_formant_bank_t *fb, float exciter_sample) -> float`

---

## 4. Token & Scope Risk Assessment

| Phase | Tasks Involved | Token Risk | Feasible with 11% Remaining? |
| :--- | :--- | :--- | :--- |
| **Phase 1: DSP Math Primitives** | Add functions to [sk_dsp.h](file:///home/stewartj/book/sk3/src/sk_dsp.h), add tests in [dsp_math_tests.c](file:///home/stewartj/book/sk3/tests/dsp_math_tests.c), run `ctest`. | **Low (~2–3%)** | **Yes** (Single file edit + 1 unit test run) |
| **Phase 2: k-synth Prototyping** | Add DSP hooks to `k-synth` playground for interactive auditioning. | **Medium (~3–4%)** | **Yes** (Contained strictly to `k-synth`) |
| **Phase 3: Full Stack `sk3` / `skode`** | Voice allocator modifications, Skode Forth words, documentation, multi-client version bumps and CI deployments across 5 repositories. | **High (~10–15%)** | **No** (High risk of exhausting quota mid-release) |
