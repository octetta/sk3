# Universal Wave Metadata & Generator Architecture

This document describes the unified metadata protocol and coordination between sound generators (`ksynth`, `tts!`, future sound DSLs) and file loaders (`/ws`, `/w`) in `sk3`.

---

## 1. Architecture Overview

In `sk3`, audio waveforms are held in the array buffer (`ands_data`) while descriptive acoustic attributes, loop points, playback modes, and cues are communicated via the string buffer (`ands_string`).

```
 +---------------------------------------------------------+
 |                      ANDS Context                       |
 |                                                         |
 |  +--------------------+       +----------------------+  |
 |  |   Audio Data       |       |   Metadata String    |  |
 |  |   (ands_data)      |       |   (ands_string)      |  |
 |  +---------+----------+       +-----------+----------+  |
 +------------|------------------------------|-------------+
              |                              |
              v                              v
     +-----------------+           +-------------------+
     | Wave Slot Audio |           | Wave Slot State   |
     | - sw.data[w]    |           | - sw.rate[w]      |
     | - sw.size[w]    |           | - sw.one_shot[w]  |
     |                 |           | - sw.loop_start[w]|
     |                 |           | - sw.loop_end[w]  |
     |                 |           | - sw.cues[w]      |
     +-----------------+           | - sw.mip_levels[w]|
                                   +-------------------+
```

When an opcode or function installs an array into a wave slot (e.g. `d>w`, `data_load`, or generator bridge), it inspects `ands_string` using the universal metadata parser `sk_parse_metadata()`.

---

## 2. Universal Metadata Protocol

Metadata strings consist of key-value pairs separated by whitespace or semicolons:

| Key | Format | Example | Description |
|---|---|---|---|
| `mode:` / `type:` | `sample` \| `osc` \| `mip` | `mode:sample` | Selects playback mode. `mode:osc` automatically computes 8-level MIP tables. `mode:sample` suppresses MIP tables and variable `A` hijacking. |
| `rate:` / `sr:` | float | `rate:44100` | Sample playback rate in Hz. |
| `one_shot:` | 0 \| 1 | `one_shot:1` | One-shot playback (1) vs sustaining cycle (0). |
| `loop:` | `start,end` or `start..end` | `loop:12000,45000` | Loop points (samples). Start is inclusive, end is exclusive. |
| `release:` | integer | `release:52000` | Sample release marker / secondary release loop. |
| `root:` / `note:` | float | `root:60` | MIDI unity / root note (e.g. 60 = Middle C). |
| `cue:` / `slice:` | `start,label` or `start..end,label` | `cue:0..13229,hello` | Cue markers or slice regions (samples). |

---

## 3. KSynth String Interpolation & Metadata

KSynth now supports string literals and dynamic runtime variable interpolation with `$VAR`:

```ksynth
/ Define loop and sample points
S: 44100
L: 12000
E: 45000

/ Declare metadata string with variable interpolation
META: "mode:sample rate:$S loop:$L,$E one_shot:1"

/ Output audio waveform
W: ...
```

### Key Behaviors:
- **Interpolation Rules**: `$A`..`$Z` and named variables are looked up dynamically at evaluation time from the active dictionary. Scalar floats are formatted as integer if whole, or `%g`. Use `$$` for a literal `$` sign.
- **Variable `META` vs `M`**: `META` is the preferred metadata identifier. Variable `M` is also checked for backward compatibility if it holds a string object. Numeric intermediate vectors assigned to `M` (such as in legacy patches) are never mistaken for metadata.
- **Variable `A` Safety**: When `mode:sample` is present, `k>w` suppresses the legacy harmonic hijacking of variable `A`, leaving harmonic tables empty and avoiding unwanted harmonic distortion on long samples.

---

## 4. TTS Marker Demarcation (`|word|`)

The `tts!` formant speech engine accepts `|marker|` tokens embedded directly in phoneme DSL strings:

```skode
[|hello| HH:50 AH0:60 L:70 OW1:90 |world| W:60 ER1:90 L:50 D:60] 300 tts!
```

### Processing:
1. `sk_phoneme_compile_dsl_markers()` extracts each marker token and records the phoneme frame index where it starts.
2. During rendering, frame durations are summed to calculate the exact sample start and end points.
3. Universal metadata is generated and stored in `ands_string`:
   ```
   mode:sample rate:44100 one_shot:1 cue:0..11465,hello cue:11466..22932,world
   ```
4. If a destination wave slot is provided (e.g. `300 tts!`), or when subsequent `d>w` is invoked, the cues are installed into `sw.cues[slot]` and `sw.num_cues[slot]`.

---

## 5. Multi-Loop and Multi-Cue WAV Loading (`/ws`, `/w`)

When loading standard WAV files through `/ws` (by name) or `/w` (by numeric asset ID), the engine parses RIFF chunks:
- **`smpl` Chunk**: Extracts MIDI unity note (`root:`) and all sample loops. Primary loop becomes `loop:start,end` and `sw.loop_start`/`sw.loop_end`. Secondary loops are stored as cues (`SK_CUE_LOOP_RELEASE`).
- **`cue ` Chunk**: Reads all cue point sample offsets.
- **`LIST` / `adtl` Chunks**: Parses `labl` (text labels for cues) and `ltxt` (slice lengths, converting cue points into slice regions).

The complete extracted metadata is stored into `ands_string(ctx->parse)` and applied to the destination wave slot.

---

## 6. Wave Diagnostics (`wavetable_show`)

Displaying wave slots (e.g. via `w?` or diagnostics) displays loop points along with any cues:

```
# W300 0.52s R/W ref#0 [data[22932]]
# playback rate 44100Hz offset +0Hz MIDI 0 mode one-shot
# loop 0..0 |0| 0ms
# cues (2): hello@0..11465 world@11466..22932
# min -0.920 max +0.941 peak 0.941 rms 0.185 dc -0.0002 zc 1420
```

---

## 7. Runtime Patch Modernization

The built-in runtime patches in `waves/` have been updated with `META:` declarations:
- **Drums** (`waves/drums/*.ks`): Declare `META: "mode:sample one_shot:1"`.
- **DW-8000** (`waves/dwg/*.ks`): Declare `META: "mode:osc"`.
- **ESQ-1** (`waves/esq1/*.ks`): Declare `META: "mode:osc loop:$L,$E"`.

---

## 8. Wave Source Classification & Modular Utilities (v2.0)

In `sk3` v2.0, wave slots are classified into six distinct source types:
- **`OSC_SOURCE_POLYBLEP`** (`w1`..`w4`, `w47`, `w48`): Closed-form continuous analytic bandlimited generation (Square/Pulse with PWM, Saw down/up, Triangle). Zero table memory.
- **`OSC_SOURCE_NOISE`** (`w5`, `w6`): Procedural noise generators (`w5` clocked LFSR chiptune noise, `w6` 64-bit continuous xorshift white noise). Zero table memory.
- **`OSC_SOURCE_CAPTURE`** (`w7`, `w8`): Live audio line-in stream (`w7` left/stereo, `w8` right).
- **`OSC_SOURCE_UTILITY`** (`w9`..`w14`): Procedural modular CV, timing, and feedback utilities:
  - `w9` (`silence`): DC 0.0 (Mute / zero reference)
  - `w10` (`unity`): DC +1.0 (Turns voice into a pure ADSR Envelope Generator / CV source)
  - `w11` (`impulse`): Single-sample Dirac click at phase reset (Filter pinging & string pluck)
  - `w12` (`metro`): BPM clock pulse / metronome tick locked to sequencer tempo
  - `w13` (`delay-tap`): Delay line audio return tap for recursive filtering and dub feedback
  - `w14` (`control`): Live MIDI CC / control plane stream
- **`OSC_SOURCE_WAVETABLE`** (`w0`, `w15`..`w46`, `w50`..`w81`): Static single-cycle periodic tables with 8-level harmonic mipmapping.
- **`OSC_SOURCE_SAMPLE`** (`w82`..`w99`, `w300`+): Planar mono and stereo PCM samples (`sw.data` left, `sw.data_right` right, `sw.channels = 2`). Stereo WAV and FLAC files de-interleave directly into planar Left/Right buffers without channel squashing.
