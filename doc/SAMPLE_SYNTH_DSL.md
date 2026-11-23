# Physical Modeling Sample Synthesis DSL & Wave Generator (`gen!`)

The **Physical Modeling Sample Synthesis DSL** (`gen!`) provides declarative procedural sample generation directly within the Skode runtime. It exposes the entire suite of `sk_dsp.h` physical modeling engines—including analog percussion circuits, modal metallic idiophones, Karplus-Strong plucked/struck strings, and resonant acoustic winds and brass—generating high-resolution audio samples into Skode data arrays and wavetable slots with universal metadata.

Companion voice synthesis is also available via **`vox!`** (shorthand alias for `tts!`), rendering bracketed phonetic speech directly to wavetables and data arrays.

---

## 1. Architecture Overview

### Why Procedural Sample Generation?
Real-time physical modeling algorithms—such as multi-delay waveguide lip-reed iterations, fractional allpass interpolations, and high-order modal resonators—require significant mathematical computation. By generating these physical models procedurally into sample slots:
1. **Zero Real-Time Overhead**: Voices play back generated wavetables with minimal CPU utilization, enabling high polyphony and tight sequencer timing.
2. **Infinite Timbral Variety**: Every acoustic nuance (strike position, mallet hardness, bore damping, lip tension, breath noise, circuit saturation) can be tuned parametrically.
3. **Universal Metadata Parity**: Every generated sample automatically carries mode, sample rate, root pitch, one-shot status, and optional loop metadata.

```
+-------------------------------------------------------------+
|               Bracketed Physical Model DSL                  |
|          [horn:brass n58 pr:0.85 br:0.7 d:1.8]              |
+-------------------------------------------------------------+
                              |
                              v  (gen!, /gen, phz!, /render)
+-------------------------------------------------------------+
|                      C DSP Generator                        |
|  - Lip-reed / Aeroacoustic / Karplus-Strong / Modal Engine  |
|  - Peak Normalization to 0 dBFS                             |
|  - Natural Acoustic Attack, Body, and Release Taper         |
+-------------------------------------------------------------+
                              |
              +---------------+---------------+
              |                               |
              v                               v
+-----------------------------+ +-----------------------------+
|   ands_data (Data Array)    | |  ands_string (Metadata)     |
|   Normalized float64 audio  | |  mode:sample rate:48000     |
|   sample buffer             | |  root:58 one_shot:1         |
+-----------------------------+ +-----------------------------+
              |                               |
              +---------------+---------------+
                              |
                              v  (slot gen! or slot d>w)
+-------------------------------------------------------------+
|                     Wavetable Engine                        |
|  - sw.data[slot]        : Audio buffer                      |
|  - sw.one_shot[slot]    : 1 (One-Shot) or 0 (Sustained)     |
|  - sw.loop_enabled[slot]: 0 (Default) or 1 (Explicit loop)  |
|  - sw.midi_note[slot]   : Root tuning note                  |
+-------------------------------------------------------------+
```

---

## 2. Invocation Opcodes & Syntax

The primary Skode command for physical modeling sample generation is **`gen!`**.
Aliases `/gen`, `phz!`, `/phz`, and `/render` are also supported for flexibility and backward compatibility.

For vocal and phonetic speech synthesis, the primary command is **`vox!`** (with aliases `/vox` and `tts!`).

### Invocation Forms

| Form | Syntax | Description |
| :--- | :--- | :--- |
| **Data Array Generate** | `[dsl] gen!` | Generates audio into active `data` array and sets formatted metadata string in `parse`. |
| **Direct Wavetable Install** | `[dsl] slot gen!` | Generates audio into `data` array and installs directly into wavetable `slot` with metadata. |
| **One-Shot Mode Override** | `[dsl] slot one_shot gen!` | Generates and installs with explicit `one_shot` flag override (`1` = one-shot, `0` = looped). |
| **Pipeline Interop with `d>w`** | `[dsl] gen!`<br>`slot d>w` | Generates into `data` array first, allowing intermediate inspection or transforms before `d>w` installs it. |

### Decay Rates vs. Explicit Buffer Duration
* **`d:` / `decay:` / `t60:`**: Controls the physical decay time constant $\tau$ of the instrument. The engine automatically sizes the generated buffer to $6.5 \times \tau + 0.15\text{s}$ (the standard acoustic $T_{60}$ boundary down to $-70\text{ dBFS}$), followed by a 15ms smooth raised-cosine taper to guarantee zero truncation clicks.
* **`dur:` / `duration:` / `len:`**: Optional explicit buffer length constraint in seconds (e.g. `dur:0.25` or `dur:2.0`).

---

## 3. The Reality of Sustain Looping vs. One-Shot Samples

### Why Physical Models Naturally Render as One-Shot Notes
Physical acoustic instruments (percussion, bells, plucked strings, wooden flutes, and brass horns) are **continuous dynamical systems** characterized by initial attack chiffs, breath turbulence, embouchure dynamics, and non-linear tube losses.

When you take an acoustic model and slice it into an arbitrary loop:
1. **Periodic Noise Becomes a Comb Filter**: Stochastic breath turbulence repeated every ~250ms turns into an artificial, metallic comb-filter buzzing drone.
2. **Phase & Pitch Jumps**: Micro-variations in lip or jet pressure cause phase velocity discontinuities across loop boundaries, producing a periodic 3–4 Hz click or stutter.

**For this reason, ALL physical models in `gen!` default to natural one-shot notes (`loop:0`, `one_shot:1`).**
They render the complete acoustic lifecycle: natural articulation, full resonant body, and a smooth natural release down to silence.

### How to Achieve Sustained Sounds
To create sounds with sustained portions that hold indefinitely:
1. **Different Stationary Spectral Models**: Clean, stationary periodic waveforms (such as single-cycle harmonic waves, drawbar organs, or wavetable pads) can loop cleanly without comb-filtering.
2. **Clever Hand-Found Loop Points with Crossfades (`loop:1`)**: If you explicitly pass `loop:1`, the engine scans for zero crossings in the steady-state section. To avoid buzzing, keep turbulence noise low (`air:0.02`), avoid baked-in pitch vibrato (apply vibrato via the voice LFO instead), and let the voice exit into the sample's release tail upon note release (`l0`).

---

## 4. Complete Instrument Catalog & Parameter Reference

### 4.1. Drums & Analog Percussion (`drum:` or `perc:`)

#### `kick` (TR-808/909 Analog Kick Drum)
* `p`, `pitch`, `f`, `start`: Start/punch pitch in Hz (default: `150.0`)
* `end`: Settled sub-bass pitch in Hz (default: `48.0`)
* `d`, `decay`: Amplitude decay in seconds (default: `0.35`)
* `pdecay`: Pitch drop decay in seconds (default: `0.035`)
* `c`, `click`: Transient beater click level `0.0`–`1.0` (default: `0.30`)
* `drive`: Cubic saturation overdrive `1.0`–`5.0` (default: `1.25`)

```skode
# Punchy 909 club kick
[drum:kick p:165 end:45 d:0.42 c:0.45 drive:2.2] 100 gen!

# Deep 808 sub bass boom
[drum:kick p:120 end:38 d:0.95 pdecay:0.06 c:0.1 drive:1.1] 101 gen!
```

#### `snare` (TR-808/909 Resonant Snare Drum)
* `f1`: Resonant body mode 1 in Hz (default: `175.0`)
* `f2`: Resonant body mode 2 in Hz (default: `224.0`)
* `snap`, `snappy`: Mix between body shell (`0.0`) and wire rattle (`1.0`) (default: `0.55`)
* `d`, `decay`: Drum body decay in seconds (default: `0.085`)
* `tail`: Snare wire noise decay in seconds (default: `0.200`)
* `f`, `freq`: Snare wire bandpass filter center in Hz (default: `2800.0`)

```skode
# Tight snappy electro snare (tail:0.18 renders full natural wire decay)
[drum:snare snap:0.85 d:0.06 tail:0.18 f:3200] 102 gen!

# Heavy deep acoustic snare
[drum:snare f1:160 f2:210 snap:0.45 d:0.12 tail:0.35] 103 gen!
```

#### `hat` (TR-808 6-Oscillator Inharmonic Cluster Hi-Hat)
* `open`: `0` for closed hi-hat, `1` for open hi-hat (default: `0`)
* `d`, `decay`: Ringing decay in seconds (default: `0.05` closed, `0.35` open)

```skode
# Crisp closed hat
[drum:hat open:0 d:0.045] 104 gen!

# Sizzling open hat
[drum:hat open:1 d:0.55] 105 gen!
```

#### `clap` (TR-808 Multi-Burst Handclap)
* `bursts`, `count`: Number of hand impact pre-bursts (default: `3`, range: `1`–`6`)
* `spread`, `space`: Spacing between claps in ms (default: `11.0`)
* `tail`: Reverberant noise tail decay in seconds (default: `0.22`)
* `f`, `freq`: Room acoustic bandpass filter in Hz (default: `1100.0`)

```skode
# Tight group clap
[drum:clap bursts:3 spread:10 tail:0.20 f:1150] 106 gen!

# Loose stadium clap
[drum:clap bursts:5 spread:18 tail:0.45 f:950] 107 gen!
```

#### `tom` (Resonant 808 Pitch-Dropping Tom)
* `f`, `n`, `pitch`: Fundamental tuning in Hz or MIDI note (default: `100.0` / G2)
* `d`, `decay`: Amplitude decay in seconds (default: `0.26`)
* `pdecay`: Pitch drop time in seconds (default: `0.045`)
* `drop`: Pitch drop factor ratio (default: `0.40`)

```skode
# Low floor tom, mid tom, high rack tom
[drum:tom n38 d:0.35 drop:0.45] 108 gen!
[drum:tom n45 d:0.28 drop:0.40] 109 gen!
[drum:tom n52 d:0.22 drop:0.35] 110 gen!
```

#### `cowbell` (TR-808 Dual Square-Wave Cowbell)
* `f1`: Oscillator 1 frequency in Hz (default: `540.0`)
* `f2`: Oscillator 2 frequency in Hz (default: `800.0`)
* `d`, `decay`: Ringing decay in seconds (default: `0.16`)
* `f`, `filter`: Bandpass resonator center in Hz (default: `800.0`)

```skode
# Classic 808 cowbell
[drum:cowbell d:0.18] 111 gen!
```

#### `membrane` / `kdrum` / `bassdrum` (2D Bessel Circular Acoustic Drum Membrane)
Physical modeling of a clamped circular drum skin vibrating across 8 Bessel partials ($1.000$, $1.593$, $2.135$, $2.295$, $2.653$, $2.917$, $3.500$, $3.598$), coupled to a pitch-drop tension relaxation envelope, shell cavity resonance, and mallet impact transient.
* `f`, `n`: Drum membrane fundamental in Hz or MIDI note (default: `80.0` / E2 for membrane, `55.0` / A1 for bassdrum)
* `d`, `decay`: Overall membrane decay time in seconds (default: `0.80`)
* `drop`: Strike impact tension jump ratio (default: `0.45` — pitch drops exponentially over ~28ms)
* `b`, `bright`: Strike position and hardness (`0.0` = center strike with deep sub boom; `1.0` = rim strike with snappy azimuthal modes)

```skode
# Resonant acoustic bass drum with deep shell thump
[drum:membrane f:55 drop:0.55 d:0.85 b:0.20] 112 gen!

# Acoustic tom / bongo membrane
[drum:membrane n62 drop:0.35 d:0.45 b:0.45] 113 gen!
```

---

### 4.2. Modal Metallic Idiophones & Bells (`bell:` or `metal:`)

Modal metallic models solve Euler-Bernoulli beam and shell mechanics, producing non-harmonic quadratic modes with mode-dependent damping and zero pitch-glide.

#### Presets
* `church`: Cathedral cast bronze bell with 10 asymmetric doublet pairs producing authentic slow acoustic beating / warble and a heavy iron clapper impact thump
* `tubular` / `chime`: Orchestral tubular bell / clock chime (Euler-Bernoulli bar flexural modes)
* `vibe` / `glock` / `marimba`: Vibraphone / glockenspiel tuned bar with mallet impact
* `tibetan` / `bowl`: Tibetan singing bowl with beating modal doublets
* `agogo` / `cow`: Latin tuned agogo bell / metallic cowbell

#### Parameters
* `f`, `n`: Fundamental strike pitch in Hz or MIDI note (e.g. `n60`, `a4`, `440.0`)
* `d`, `decay`: T60 decay scale multiplier (e.g. `d:2.5` = 2.5x longer ringing; church bell rings naturally for ~5.5s)
* `m`, `mallet`: Mallet hardness / transient strike click level `0.0`–`1.0` (default: `0.40`)
* `loop`: `0` for one-shot (default), `1` for sustain loop

```skode
# Cathedral cast bronze bell (rings naturally for ~5.5s with doublet warble and clapper thump)
[metal:church n57 d:1.0 m:0.6] 120 gen!

# Orchestral tubular clock chime
[bell:tubular n72 d:1.0 m:0.4] 121 gen!

# Concert vibraphone bar
[metal:vibe n65 d:1.0 m:0.35] 122 gen!

# Meditative Tibetan singing bowl
[bell:bowl n48 d:1.0] 123 gen!

# High Latin agogo bell
[metal:agogo n76 d:1.0 m:0.7] 124 gen!
```

---

### 4.3. Plucked & Struck Strings (`string:` or `pluck:`)

Karplus-Strong physical modeling with fractional allpass interpolation, comb-filter pick positioning, one-pole loop loss damping, and **dual-biquad acoustic wooden soundbox body resonance** (Helmholtz air cavity + top/back soundboard wood modes) for upright bass and cello.

#### Presets
* `guitar`: Acoustic guitar (`pick:0.25`, `damp:0.45`, `fb:0.994`, `bright:0.70`)
* `bass`: Upright acoustic double bass with wooden soundbox body resonator (62 Hz air cavity + 98 Hz & 165 Hz wood modes)
* `pizz` / `cello`: Orchestral cello pizzicato with wooden soundbox body resonator (125 Hz air cavity + 220 Hz & 380 Hz wood modes)
* `harp`: Concert harp (`pick:0.50`, `damp:0.25`, `fb:0.997`, `bright:0.80`)
* `koto` / `zither`: Japanese koto (`pick:0.12`, `damp:0.30`, `fb:0.992`, `bright:0.75`)
* `sitar`: Indian sitar with jawari bridge buzz (`pick:0.15`, `damp:0.65`, `fb:0.990`, `bright:0.85`)
* `raw` / `clean`: Unfiltered Karplus-Strong string

#### Parameters
* `f`, `n`: Fundamental string tuning in Hz or MIDI note (e.g. `n60`, `c4`, `261.63`)
* `p`, `pick`: Pick position along string `0.02`–`0.98` (`0.50` = center/fluty, `0.10` = bridge/twangy)
* `damp`: High-frequency string loss damping `0.05`–`0.95` (higher = darker/muted)
* `fb`, `decay`: Loop feedback coefficient `0.85`–`0.999` (governs ring sustain)
* `b`, `bright`: Excitation pulse brightness `0.0`–`1.0` (soft finger vs sharp plectrum)
* `vel`: Strike velocity `0.1`–`1.0` (default: `1.0`)

```skode
# Spanish acoustic nylon guitar
[string:guitar n64 p:0.28 damp:0.40 fb:0.994 b:0.75] 130 gen!

# Deep upright acoustic bass with resonant wooden cavity
[pluck:bass n28 p:0.25 damp:0.35 fb:0.996 b:0.45] 131 gen!

# Orchestral pizzicato cello
[string:pizz n48 p:0.30 damp:0.40 fb:0.992 b:0.50] 132 gen!

# Concert harp plucked at center string
[string:harp n72 p:0.50 damp:0.20 fb:0.998 b:0.85] 133 gen!

# Bright twangy Japanese koto
[string:koto n67 p:0.10 damp:0.25 fb:0.993 b:0.80] 133 gen!

# Indian sitar
[string:sitar n55 p:0.14 damp:0.60 fb:0.991 b:0.90] 134 gen!

# Orchestral cello pizzicato
[string:pizz n48 p:0.40 damp:0.75 fb:0.982 b:0.35] 135 gen!
```

---

### 4.4. Horns, Brass & Aeroacoustic Flutes (`horn:`, `brass:`, `wind:`, `flute:`)

Resonant acoustic pipe models rendering natural, organic wind notes with dynamic mouth/blowing pressure and smooth breath release tapers.

#### `brass` (Lip-Reed Horn, Trumpet, Trombone, Tuba)
Models lip-reed pulse mechanics with pressure-dependent duty cycle, non-linear shock wave steepening (Burgers' effect), and 3-cavity horn formants (580 Hz bore, 1450 Hz throat, 2800 Hz flared bell radiator).
* `f`, `n`: Fundamental tuning in Hz or MIDI note (default: `n58` / Bb3)
* `pr`, `press`: Mouth blowing pressure `0.2`–`1.0` (`0.4` = piano horn, `0.85` = fortissimo blare)
* `br`, `brass`: Brassiness non-linear factor `0.0`–`1.0` (default: `0.50`)
* `damp`: Bore wall loss damping `0.05`–`0.60` (default: `0.25`)
* `dur`, `d`: Render duration in seconds (default: `1.8`)
* `loop`: `0` (default one-shot note) or `1` (sustain loop)

```skode
# Brilliant Bb trumpet lead with fortissimo blare
[horn:brass n58 pr:0.88 br:0.75] 140 gen!

# Warm French horn / tenor trombone
[horn:brass n46 pr:0.65 br:0.35 damp:0.30] 141 gen!

# Heavy orchestral tuba
[horn:brass n34 pr:0.80 br:0.60 damp:0.35] 142 gen!
```

#### `flute` (Aeroacoustic Cylindrical Pipe, Recorder, Flute)
Models open cylindrical pipe resonance with dominant odd harmonics (1/n harmonic roll-off), initial 35ms attack chiff, gentle lowpassed breath air, and wooden body cavity resonance (1100 Hz).
* `f`, `n`: Fundamental tuning in Hz or MIDI note (default: `n72` / C5)
* `pr`, `press`: Blowing pressure `0.2`–`1.2` (`0.75` = standard, `>0.85` = smooth octave jump)
* `air`, `noise`: Breath turbulence noise level `0.0`–`0.2` (default: `0.05`)
* `damp`: Bore wall reflection loss `0.05`–`0.80` (default: `0.25`)
* `dur`, `d`: Render duration in seconds (default: `1.8`)
* `loop`: `0` (default one-shot note) or `1` (sustain loop)

```skode
# Expressive concert flute note
[wind:flute n72 pr:0.75 air:0.04] 143 gen!

# Breathy wooden pipe
[wind:flute n60 pr:0.68 air:0.08 damp:0.35] 144 gen!

# Overblown octave flute register
[wind:flute n60 pr:0.95 air:0.05] 145 gen!
```

---

## 5. Wealth of Production Examples

### Example 1: Procedural Drum Kit & Sequencer Pattern
Generate procedural percussion across wavetable slots 100–103, assign voices, and drive them with an 8-step sequencer pattern featuring syncopated ghost hits:

```skode
# 1. Generate drum kit instruments with natural acoustic tails
[drum:kick p:160 end:45 d:0.42 drive:1.8] 100 gen!
[drum:snare snap:0.85 d:0.08 tail:0.22] 101 gen!
[drum:hat open:0 d:0.04] 102 gen!
[drum:hat open:1 d:0.45] 103 gen!

# 2. Assign wave slots, set 0dB gain, and native sample playback pitch (n69)
v0 w100 a0 n69
v1 w101 a0 n69
v2 w102 a0 n69
v3 w103 a0 n69

# 3. Eight-step sequenced drum beat pattern with swing ghost hit (+.25) on hat
y0 %2 
[v0 l1] x0
[v2 l1] x1
[v1 l1] x2
[v2 l1] x3
[v0 l1] x4
[v2 l1 +.25 l1] x5
[v1 l1] x6
[v3 l1] x7
```

---

### Example 2: Natural Acoustic Melodic Phrases
Render expressive brass and concert flute notes as one-shot physical models and play sequential melodic phrases using canonical single-line syntax with inline `~` timed rests:

```skode
# 1. Generate natural acoustic brass and flute notes
[horn:brass n58 pr:0.85 br:0.70] 110 gen!
[wind:flute n72 pr:0.75 air:0.04] 111 gen!

# 2. Play trumpet fanfare on voice 0 with inline timed rests
v0 w110 a0 n58 l1 ~.4 n62 l1 ~.4 n65 l1 ~.8 n70 l1

# 3. Play expressive concert flute phrase on voice 1
v1 w111 a0 n72 l1 ~.5 n75 l1 ~.5 n77 l1 ~.75 n72 l1
```

---

### Example 3: Tibetan Singing Bowl & Drone Pad
Render an acoustic Tibetan singing bowl with long modal resonance:

```skode
# Generate singing bowl note (or explicit loop:1 for continuous drone)
[bell:bowl n48 d:2.5] 120 gen!

# Polyphonic drone chord (C3 + G3 + C4)
v0 w120 a-3 n48 l1
v1 w120 a-6 n55 l1
v2 w120 a-6 n60 l1
```

---

### Example 4: Speech Synthesis with `vox!`
Render spoken word phonemes directly into wavetable slots using the `vox!` opcode:

```skode
# Render phonemes into wave slot 150 using vox!
[hh eh l ow w er l d] 150 vox!

# Play speech on voice 2 at native speed
v2 w150 a0 n69 l1
```

---

### Example 5: Pipeline Data Editing with `d>w`
Because `gen!` populates the active `data` buffer and formats metadata into `parse`, users can inspect or apply mathematical operations before installing to a wavetable:

```skode
# 1. Generate raw acoustic nylon guitar into data buffer
[string:guitar n60 p:0.25 damp:0.4 fb:0.995] gen!

# 2. Inspect data array length or apply custom transformation
?d

# 3. Install into wavetable slot 130 (metadata carries over automatically)
130 d>w

# 4. Play acoustic guitar note on voice 0
v0 w130 a0 n60 l1
```

---

### Example 6: Loop Finder Tool (`/findloop`, `?loop`, `WL?`)
When working with sustained acoustic sounds (brass, flute, bowed strings, or loaded audio samples), finding click-free zero-crossing loop points manually can be tedious. The `/findloop` tool automates this process:

```skode
# 1. Render a sustained trumpet sample as a natural one-shot
[horn:brass n58 pr:0.85 dur:1.8] 140 gen!

# 2. Automatically detect optimal click-free sustain loop points
140 /findloop
# Output: # findloop: w140 loop points set to 15420..31860 (len=16440 samples, 342.50 ms, corr=0.985)

# 3. The loop points and loop mode are automatically applied to slot 140,
# while the formatted metadata string is stored in the parse string buffer:
# [mode:sample loop:15420,31860 one_shot:0 rate:48000 root:58]

# 4. Trigger voice 0 with continuous sustain
v0 w140 a0 n58 l1
```

Aliases supported:
* `slot /findloop` / `/findloop` (uses active voice's wavetable if slot omitted)
* `slot ?loop` / `?loop`
* `slot WL?` / `WL?`

---

### Example 7: KSynth Metadata & Array DSP Pipeline Interop
The parser `data` buffer, `parse` metadata string, wavetable slots `W`, and KSynth array DSP form a unified two-way pipeline:

```skode
# 1. Generate physical modeling instrument into data buffer
[drum:snare snap:0.85 d:0.08 tail:0.25] gen!

# 2. Load the generated samples directly into KSynth array variable A
0 d>k

# 3. Execute array DSP in KSynth (e.g. reverse, filter, zero-crossing detection)
# Dyadic x finds zero-crossings for splicing:
# Z: A x 0.001

# 4. Alternatively, transfer an existing wavetable slot 140 into KSynth variable B
140 1 w>k

# 5. Route KSynth results back into data buffer or wavetable slots:
# All metadata (loop points, sample rate, root note, one-shot flag) remains
# intact and synchronized across the execution environment.
```

---

## 6. Summary of Default Settings & Aliases

| Preset | Family | Pitch Default | Duration Default | Loop Default | One-Shot Default |
| :--- | :--- | :--- | :--- | :--- | :--- |
| `kick` | `drum` | 150 Hz $\to$ 48 Hz | ~0.55s | `0` (off) | `1` (one-shot) |
| `snare` | `drum` | 175 Hz + 224 Hz | ~0.41s | `0` (off) | `1` (one-shot) |
| `hat` | `drum` | Cluster | 0.12s / 0.72s | `0` (off) | `1` (one-shot) |
| `clap` | `drum` | 1100 Hz BP | ~0.52s | `0` (off) | `1` (one-shot) |
| `tom` | `drum` | 100 Hz (G2) | ~0.57s | `0` (off) | `1` (one-shot) |
| `cowbell` | `drum` | 540 Hz + 800 Hz | ~0.34s | `0` (off) | `1` (one-shot) |
| `membrane` | `drum` | 80 Hz | 0.80s | `0` (off) | `1` (one-shot) |
| `church` | `bell` | 220 Hz (A3) | 5.50s | `0` (off) | `1` (one-shot) |
| `tubular` | `bell` | 440 Hz (A4) | 3.00s | `0` (off) | `1` (one-shot) |
| `vibe` | `bell` | 440 Hz (A4) | 2.50s | `0` (off) | `1` (one-shot) |
| `tibetan` | `bell` | 220 Hz (A3) | 4.00s | `0` (off) | `1` (one-shot) |
| `agogo` | `bell` | 587.33 Hz (D5) | 0.80s | `0` (off) | `1` (one-shot) |
| `guitar` | `string` | 261.63 Hz (C4) | 2.00s | `0` (off) | `1` (one-shot) |
| `bass` | `string` | 82.41 Hz (E2) | 2.00s | `0` (off) | `1` (one-shot) |
| `harp` | `string` | 440 Hz (A4) | 2.00s | `0` (off) | `1` (one-shot) |
| `koto` | `string` | 329.63 Hz (E4) | 2.00s | `0` (off) | `1` (one-shot) |
| `sitar` | `string` | 220 Hz (A3) | 2.00s | `0` (off) | `1` (one-shot) |
| `pizz` | `string` | 440 Hz (A4) | 2.00s | `0` (off) | `1` (one-shot) |
| `brass` | `horn` | 233.08 Hz (Bb3) | 1.80s | `0` (off) | `1` (one-shot) |
| `flute` | `wind` | 523.25 Hz (C5) | 1.80s | `0` (off) | `1` (one-shot) |
