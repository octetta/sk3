# Physical Modeling Sample Synthesis DSL & Wave Renderer

The **Physical Modeling Sample Synthesis DSL** provides declarative, offline procedural sample rendering directly within the Skode runtime. It exposes the entire suite of `sk_dsp.h` physical modeling engines—including analog percussion circuits, modal metallic idiophones, Karplus-Strong plucked/struck strings, and non-linear aeroacoustic winds and brass—rendering full-scale audio samples into Skode data arrays and wavetable slots with universal metadata.

---

## 1. Architecture Overview

### Why Procedural Sample Rendering?
Real-time physical modeling algorithms—such as multi-delay waveguide lip-reed iterations, fractional allpass interpolations, and high-order modal resonators—require significant mathematical computation. By rendering these physical models procedurally into sample slots:
1. **Zero Real-Time Overhead**: Voices play back rendered wavetables with minimal CPU utilization, enabling high polyphony and tight sequencer timing.
2. **Infinite Timbral Variety**: Every acoustic nuance (strike position, mallet hardness, bore damping, lip tension, breath noise, circuit saturation) can be tuned parametrically.
3. **Universal Metadata Parity**: Every rendered sample automatically carries mode, sample rate, root pitch, one-shot status, and period-aligned sustain loop points.

```
+-------------------------------------------------------------+
|               Bracketed Physical Model DSL                  |
|          [horn:brass n58 pr:0.85 br:0.7 d:1.5]              |
+-------------------------------------------------------------+
                              |
                              v  (/render, render!, /dsp)
+-------------------------------------------------------------+
|                      C DSP Generator                        |
|  - Lip-reed / Aeroacoustic / Karplus-Strong / Modal Engine  |
|  - Peak Normalization to 0 dBFS                             |
|  - Phase-Coherent Sustain Loop Point Extraction             |
+-------------------------------------------------------------+
                              |
              +---------------+---------------+
              |                               |
              v                               v
+-----------------------------+ +-----------------------------+
|   ands_data (Data Array)    | |  ands_string (Metadata)     |
|   Normalized float64 audio  | |  mode:sample rate:48000     |
|   sample buffer             | |  root:58 loop:9614,24035    |
+-----------------------------+ +-----------------------------+
              |                               |
              +---------------+---------------+
                              |
                              v  (slot /render or slot d>w)
+-------------------------------------------------------------+
|                     Wavetable Engine                        |
|  - sw.data[slot]        : Audio buffer                      |
|  - sw.one_shot[slot]    : 0 (Sustained) or 1 (One-Shot)     |
|  - sw.loop_enabled[slot]: 1                                 |
|  - sw.loop_start[slot]  : Zero-crossing attack end          |
|  - sw.loop_end[slot]    : Phase-matched cycle boundary      |
|  - sw.midi_note[slot]   : Root tuning note                  |
+-------------------------------------------------------------+
```

---

## 2. Invocation Opcodes & Syntax

The primary Skode command for physical modeling sample rendering is **`phz!`** (shorthand for *physical*).
Backward-compatible aliases `/phz` and `/render` are also supported.

### Invocation Forms

| Form | Syntax | Description |
| :--- | :--- | :--- |
| **Data Array Render** | `[dsl] phz!` | Renders audio into active `data` array and sets formatted metadata string in `parse`. |
| **Direct Wavetable Install** | `[dsl] slot phz!` | Renders audio into `data` array and installs directly into wavetable `slot` with metadata. |
| **One-Shot Mode Override** | `[dsl] slot one_shot phz!` | Renders and installs with explicit `one_shot` flag override (`1` = one-shot, `0` = looped). |
| **Pipeline Interop with `d>w`** | `[dsl] phz!`<br>`slot d>w` | Renders into `data` array first, allowing intermediate inspection or transforms before `d>w` installs it. |

### Decay Rates vs. Explicit Buffer Duration
* **`d:` / `decay:` / `t60:`**: Controls the physical decay time constant $\tau$ of the instrument. The engine automatically sizes the rendered buffer to $6.5 \times \tau + 0.15\text{s}$ (the standard acoustic $T_{60}$ boundary down to $-70\text{ dBFS}$), followed by a 15ms smooth raised-cosine taper to guarantee zero truncation click or clipping.
* **`dur:` / `duration:` / `len:`**: Optional explicit buffer length constraint in seconds (e.g. `dur:0.25` or `dur:2.0`). Use this only when you deliberately want to constrain the sample buffer length.

---

## 3. Sustain Looping & Loop Metadata Architecture

Acoustic instruments fall into two distinct physical categories:
1. **Impulsive / Percussive Instruments** (`kick`, `snare`, `hat`, `clap`, `tom`, `guitar`, `harp`, `church`):
   Excited by an initial impulse or strike, decaying naturally to silence. These default to `one_shot:1` and `loop:0`.
2. **Continuous-Excitation Instruments** (`brass`, `flute`, `tibetan` bowl, sustained `string`):
   Excited continuously by blowing pressure or rubbing friction. These default to `one_shot:0` and `loop:1`.

### Period-Aligned Zero-Crossing Loop Extraction
For sustained instruments, the generator executes automated loop point extraction:
1. **Transient Guard**: Skips the initial attack and embouchure-settling transient ($~18\text{--}20\%$ of the audio buffer).
2. **Positive-Going Zero Crossing**: Identifies the first positive-going zero crossing ($s[i] \le 0 < s[i+1]$) as `loop_start`.
3. **Integer Period Calculation**: Determines an integer number of fundamental cycles $k$ matching target sustain window duration ($~250\text{--}350\text{ ms}$):
   $$k = \text{round}\left(T_{\text{loop}} \times f_0\right)$$
4. **Derivative & Slope Matching**: Scans a window of $\pm 0.6 \times \text{period}$ around the nominal end point to locate a positive-going zero crossing that minimizes slope difference:
   $$\Delta_{\text{diff}} = \left| s'[j] - s'[\text{loop\_start}] \right| + \left| s[j] - s[\text{loop\_start}] \right|$$
5. **Phase-Coherent Loop Setup**: Assigns `loop_start` and `loop_end`. When assigned to a voice (`v0 w101 a0 n69 l1`), triggering the note loops indefinitely within the sustain area without clicks, popping, or comb filtering. When the note is released (`l0`), the engine exits the loop and plays through the natural acoustic release tail to completion.

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
[drum:kick p:165 end:45 d:0.42 c:0.45 drive:2.2] 100 phz!

# Deep 808 sub bass boom
[drum:kick p:120 end:38 d:0.95 pdecay:0.06 c:0.1 drive:1.1] 101 phz!
```

#### `snare` (TR-808/909 Resonant Snare Drum)
* `f1`: Resonant body mode 1 in Hz (default: `175.0`)
* `f2`: Resonant body mode 2 in Hz (default: `224.0`)
* `snap`, `snappy`: Mix between body shell (`0.0`) and wire rattle (`1.0`) (default: `0.55`)
* `d`, `decay`: Drum body decay in seconds (default: `0.085`)
* `tail`: Snare wire noise decay in seconds (default: `0.200`)
* `f`, `freq`: Snare wire bandpass filter center in Hz (default: `2800.0`)

```skode
# Tight snappy electro snare (tail:0.18 renders full 1.32s natural wire decay)
[drum:snare snap:0.85 d:0.06 tail:0.18 f:3200] 102 phz!

# Heavy deep acoustic snare
[drum:snare f1:160 f2:210 snap:0.45 d:0.12 tail:0.35] 103 phz!
```

#### `hat` (TR-808 6-Oscillator Inharmonic Cluster Hi-Hat)
* `open`: `0` for closed hi-hat, `1` for open hi-hat (default: `0`)
* `d`, `decay`: Ringing decay in seconds (default: `0.05` closed, `0.35` open)

```skode
# Crisp closed hat
[drum:hat open:0 d:0.045] 104 phz!

# Sizzling open hat
[drum:hat open:1 d:0.55] 105 phz!
```

#### `clap` (TR-808 Multi-Burst Handclap)
* `bursts`, `count`: Number of hand impact pre-bursts (default: `3`, range: `1`–`6`)
* `spread`, `space`: Spacing between claps in ms (default: `11.0`)
* `tail`: Reverberant noise tail decay in seconds (default: `0.22`)
* `f`, `freq`: Room acoustic bandpass filter in Hz (default: `1100.0`)

```skode
# Tight group clap
[drum:clap bursts:3 spread:10 tail:0.20 f:1150] 106 phz!

# Loose spacious stadium clap
[drum:clap bursts:5 spread:18 tail:0.45 f:950] 107 phz!
```

#### `tom` (Resonant 808 Pitch-Dropping Tom)
* `f`, `n`, `pitch`: Fundamental tuning in Hz or MIDI note (default: `100.0` / G2)
* `d`, `decay`: Amplitude decay in seconds (default: `0.26`)
* `pdecay`: Pitch drop time in seconds (default: `0.045`)
* `drop`: Pitch drop factor ratio (default: `0.40`)

```skode
# Low floor tom, mid tom, high rack tom
[drum:tom n38 d:0.35 drop:0.45] 108 phz!
[drum:tom n45 d:0.28 drop:0.40] 109 phz!
[drum:tom n52 d:0.22 drop:0.35] 110 phz!
```

#### `cowbell` (TR-808 Dual Square-Wave Cowbell)
* `f1`: Oscillator 1 frequency in Hz (default: `540.0`)
* `f2`: Oscillator 2 frequency in Hz (default: `800.0`)
* `d`, `decay`: Ringing decay in seconds (default: `0.16`)
* `f`, `filter`: Bandpass resonator center in Hz (default: `800.0`)

```skode
# Classic 808 cowbell
[drum:cowbell d:0.18] 111 phz!
```

#### `membrane` / `kdrum` (Karplus-Strong 1983 Acoustic Drum)
* `f`, `n`: Drum fundamental in Hz or MIDI note (default: `80.0`)
* `blend`: `0.5` = acoustic drum membrane, `1.0` = string, `0.0` = noise (default: `0.50`)
* `damp`: Loss damping factor (default: `0.55`)
* `fb`, `decay`: Feedback decay factor (default: `0.988`)
* `b`, `bright`: Impact strike brightness (default: `0.50`)

```skode
# Deep acoustic bass drum
[drum:membrane n36 blend:0.5 damp:0.6 fb:0.985 b:0.4] 112 phz!

# Resonant acoustic bongo / conga
[drum:membrane n62 blend:0.55 damp:0.3 fb:0.992 b:0.75] 113 phz!
```

---

### 4.2. Modal Metallic Idiophones & Bells (`bell:` or `metal:`)

Modal metallic models solve Euler-Bernoulli beam and shell mechanics, producing non-harmonic quadratic modes with mode-dependent damping and zero pitch-glide.

#### Presets
* `church`: Tuned bronze church bell (Hum, Prime, Tierce, Quint, Nominal, Decim)
* `tubular` / `chime`: Orchestral tubular bell / clock chime (Euler-Bernoulli bar flexural modes)
* `vibe` / `glock` / `marimba`: Vibraphone / glockenspiel tuned bar with mallet impact
* `tibetan` / `bowl`: Tibetan singing bowl with beating modal doublets (`loop:1` by default)
* `agogo` / `cow`: Latin tuned agogo bell / metallic cowbell

#### Parameters
* `f`, `n`: Fundamental strike pitch in Hz or MIDI note (e.g. `n60`, `a4`, `440.0`)
* `d`, `decay`: T60 decay scale multiplier (e.g. `d:2.5` = 2.5x longer ringing)
* `m`, `mallet`: Mallet hardness / transient strike click level `0.0`–`1.0` (default: `0.40`)
* `loop`: `0` for one-shot, `1` for sustain loop (default: `1` for `tibetan`, `0` for others)

```skode
# Cathedral bronze bell (rings naturally for ~5.5s)
[metal:church n57 d:1.0 m:0.6] 120 phz!

# Orchestral tubular clock chime
[bell:tubular n72 d:1.0 m:0.4] 121 phz!

# Concert vibraphone bar
[metal:vibe n65 d:1.0 m:0.35] 122 phz!

# Meditative Tibetan singing bowl with continuous sustain loop
[bell:bowl n48 d:1.0 loop:1] 123 phz!

# High Latin agogo bell
[metal:agogo n76 d:1.0 m:0.7] 124 phz!
```

---

### 4.3. Plucked & Struck Strings (`string:` or `pluck:`)

Karplus-Strong physical modeling with fractional allpass interpolation, comb-filter pick positioning, and one-pole loop loss damping.

#### Presets
* `guitar`: Acoustic guitar (`pick:0.25`, `damp:0.45`, `fb:0.994`, `bright:0.70`)
* `bass`: Acoustic/electric bass (`pick:0.35`, `damp:0.55`, `fb:0.995`, `bright:0.50`)
* `harp`: Concert harp (`pick:0.50`, `damp:0.25`, `fb:0.997`, `bright:0.80`)
* `koto` / `zither`: Japanese koto (`pick:0.12`, `damp:0.30`, `fb:0.992`, `bright:0.75`)
* `sitar`: Indian sitar with jawari bridge buzz (`pick:0.15`, `damp:0.65`, `fb:0.990`, `bright:0.85`)
* `pizz`: Orchestral violin/cello pizzicato (`pick:0.40`, `damp:0.70`, `fb:0.980`, `bright:0.40`)
* `raw` / `clean`: Unfiltered Karplus-Strong string

#### Parameters
* `f`, `n`: Fundamental string tuning in Hz or MIDI note (e.g. `n60`, `c4`, `261.63`)
* `p`, `pick`: Pick position along string `0.02`–`0.98` (`0.50` = center/fluty, `0.10` = bridge/twangy)
* `damp`: High-frequency string loss damping `0.05`–`0.95` (higher = darker/muted)
* `fb`, `decay`: Loop feedback coefficient `0.85`–`0.999` (governs ring sustain)
* `b`, `bright`: Excitation pulse brightness `0.0`–`1.0` (soft finger vs sharp plectrum)
* `vel`: Strike velocity `0.1`–`1.0` (default: `1.0`)
* `loop`: `0` = one-shot pluck, `1` = looped string / E-Bow sustain

```skode
# Spanish acoustic nylon guitar
[string:guitar n64 p:0.28 damp:0.40 fb:0.994 b:0.75] 130 phz!

# Deep upright acoustic bass
[pluck:bass n36 p:0.35 damp:0.55 fb:0.996 b:0.45] 131 phz!

# Concert harp plucked at center string
[string:harp n72 p:0.50 damp:0.20 fb:0.998 b:0.85] 132 phz!

# Bright twangy Japanese koto
[string:koto n67 p:0.10 damp:0.25 fb:0.993 b:0.80] 133 phz!

# Indian sitar
[string:sitar n55 p:0.14 damp:0.60 fb:0.991 b:0.90] 134 phz!

# Orchestral cello pizzicato
[string:pizz n48 p:0.40 damp:0.75 fb:0.982 b:0.35] 135 phz!
```

---

### 4.4. Horns, Brass & Aeroacoustic Flutes (`horn:`, `brass:`, `wind:`, `flute:`)

Physical continuous waveguides driven by non-linear lip-reed valves and aeroacoustic jet splits. **Both `brass` and `flute` automatically extract seamless sustain loops by default (`loop:1`).**

#### `brass` (Lip-Reed Horn, Trumpet, Trombone, Tuba)
Implements Perry Cook and Fletcher & Rossing mass-spring lip mechanics coupled to a flared horn bell radiator with Burgers' equation non-linear shock wave steepening.
* `f`, `n`: Fundamental tuning in Hz or MIDI note (default: `n58` / Bb3)
* `pr`, `press`: Mouth blowing pressure `0.2`–`1.0` (`0.4` = piano horn, `0.85` = fortissimo blare)
* `br`, `brass`: Brassiness non-linear factor `0.0`–`1.0` (default: `0.50`)
* `damp`: Bore reflection wall loss damping `0.05`–`0.60` (default: `0.25`)
* `dur`, `d`: Render duration in seconds (default: `1.5`)
* `loop`: `1` (default; automatically computes seamless sustain loop)

```skode
# Brilliant Bb trumpet lead with fortissimo blare
[horn:brass n58 pr:0.88 br:0.75] 140 phz!

# Warm French horn / tenor trombone
[horn:brass n46 pr:0.65 br:0.35 damp:0.30] 141 phz!

# Heavy orchestral tuba
[horn:brass n34 pr:0.80 br:0.60 damp:0.35] 142 phz!
```

#### `flute` (Aeroacoustic Jet Pipe, Recorder, Flute)
Implements Coltman and MSW jet-drive physics with labium knife-edge pressure splitting, breath turbulence noise, natural 5.5 Hz vibrato, and overblowing.
* `f`, `n`: Fundamental tuning in Hz or MIDI note (default: `n72` / C5)
* `pr`, `press`: Blowing pressure `0.2`–`1.2` (`0.7` = standard, `>0.9` = register jump / overblow)
* `air`, `noise`: Breath turbulence noise level `0.0`–`0.5` (default: `0.12`)
* `vib`: Natural vibrato depth `0.0`–`0.1` (default: `0.03`)
* `vibr`: Vibrato frequency in Hz (default: `5.5`)
* `damp`: Bore wall reflection loss `0.05`–`0.80` (default: `0.35`)
* `dur`, `d`: Render duration in seconds (default: `1.5`)
* `loop`: `1` (default; automatically computes seamless sustain loop)

```skode
# Expressive concert flute with natural vibrato
[wind:flute n72 pr:0.72 air:0.10 vib:0.04] 143 phz!

# Breathy Japanese shakuhachi / wooden pipe
[wind:flute n60 pr:0.68 air:0.25 vib:0.06 damp:0.45] 144 phz!

# Overblown octave flute register
[wind:flute n60 pr:0.98 air:0.14 vib:0.03] 145 phz!
```

---

## 5. Wealth of Production Examples

### Example 1: Procedural 808/909 Drum Kit Setup
Create a full drum kit procedurally across wavetable slots 100–106 and assign voices using canonical Skode commands:

```skode
# 1. Render all drum kit instruments with full natural tails
[drum:kick p:160 end:45 d:0.42 drive:1.8] 100 phz!
[drum:snare snap:0.85 d:0.08 tail:0.22] 101 phz!
[drum:hat open:0 d:0.04] 102 phz!
[drum:hat open:1 d:0.45] 103 phz!
[drum:clap bursts:3 spread:11 tail:0.22] 104 phz!
[drum:tom n42 d:0.32 drop:0.45] 105 phz!
[drum:cowbell d:0.16] 106 phz!

# 2. Assign voices (assign wave, set 0dB gain, set native sample pitch n69, trigger with l1)
v0 w100 a0 n69 l1   # Kick on voice 0
v1 w101 a0 n69 l1   # Snare on voice 1
v2 w102 a0 n69 l1   # Closed hat on voice 2
v3 w103 a0 n69 l1   # Open hat on voice 3
```

---

### Example 2: Sustained Horn & Flute Lead with Natural Acoustic Release
Render a brass trumpet and wooden flute with seamless sustain looping. When triggered (`l1`), the voice sustains indefinitely within the zero-crossing loop area. When released (`l0`), the engine automatically exits the loop and plays through the natural acoustic release tail to completion—no artificial ADSR envelope is needed because the acoustic decay is built into the physical sample itself:

```skode
# 1. Render sustained brass and flute with automated sustain loop metadata
[horn:brass n58 pr:0.85 br:0.70] 110 phz!
[wind:flute n72 pr:0.72 air:0.15] 111 phz!

# 2. Play sustained brass lead on voice 0
v0 w110 a0 n58 l1   # Trigger Bb3 lead; sustains seamlessly inside loop
~2.0                # Hold for 2 seconds
v0 l0               # Gate off: engine exits loop and plays natural acoustic release tail

# 3. Play expressive flute melodic line on voice 1
v1 w111 a0 n72 l1   # Trigger root C5
~0.5
v1 n75              # Legato slur to Eb5 (stays sustained inside loop)
~0.5
v1 n77              # Legato slur to F5
~0.75
v1 n72              # Return to C5
~0.5
v1 l0               # Release note; plays natural acoustic breath tail
```

---

### Example 3: Tibetan Singing Bowl Meditative Drone Pad
Combine a sustained Tibetan singing bowl with pitch transposition across octaves:

```skode
# Render singing bowl with sustain loop enabled
[bell:bowl n48 d:1.0 loop:1] 120 phz!

# Polyphonic drone chord (C3 + G3 + C4)
v0 w120 a-3 n48 l1
v1 w120 a-6 n55 l1
v2 w120 a-6 n60 l1

# Release voices after sustained meditation
~4.0
v0 l0
v1 l0
v2 l0
```

---

### Example 4: Pipeline Data Editing with `d>w`
Because `phz!` populates the active `data` buffer and formats metadata into `parse`, users can apply mathematical operations before installing to a wavetable:

```skode
# 1. Render raw acoustic nylon guitar into data buffer
[string:guitar n60 p:0.25 damp:0.4 fb:0.995] phz!

# 2. Inspect data array length or apply custom transformation
?d

# 3. Install into wavetable slot 130 (metadata carries over automatically)
130 d>w

# 4. Play acoustic guitar note on voice 0
v0 w130 a0 n60 l1
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
| `church` | `bell` | 220 Hz (A3) | 3.50s | `0` (off) | `1` (one-shot) |
| `tubular` | `bell` | 440 Hz (A4) | 3.00s | `0` (off) | `1` (one-shot) |
| `vibe` | `bell` | 440 Hz (A4) | 2.50s | `0` (off) | `1` (one-shot) |
| `tibetan` | `bell` | 220 Hz (A3) | 4.00s | **`1` (on)** | **`0` (sustained)** |
| `agogo` | `bell` | 587.33 Hz (D5) | 0.80s | `0` (off) | `1` (one-shot) |
| `guitar` | `string` | 261.63 Hz (C4) | 2.00s | `0` (off) | `1` (one-shot) |
| `bass` | `string` | 82.41 Hz (E2) | 2.00s | `0` (off) | `1` (one-shot) |
| `harp` | `string` | 440 Hz (A4) | 2.00s | `0` (off) | `1` (one-shot) |
| `koto` | `string` | 329.63 Hz (E4) | 2.00s | `0` (off) | `1` (one-shot) |
| `sitar` | `string` | 220 Hz (A3) | 2.00s | `0` (off) | `1` (one-shot) |
| `pizz` | `string` | 440 Hz (A4) | 2.00s | `0` (off) | `1` (one-shot) |
| `brass` | `horn` | 233.08 Hz (Bb3) | 1.20s | **`1` (on)** | **`0` (sustained)** |
| `flute` | `wind` | 523.25 Hz (C5) | 1.20s | **`1` (on)** | **`0` (sustained)** |
