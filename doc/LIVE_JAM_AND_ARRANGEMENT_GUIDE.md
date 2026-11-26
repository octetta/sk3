# sk3 Music Composition & Live Jamming Guide

A comprehensive tutorial, arrangement architecture guide, and performance reference for composing tracks and performing live electronic music in **sk3**.

---

## Table of Contents
1. [Core Concepts & Architecture](#1-core-concepts--architecture)
2. [Step 1: Sound Design & Kits](#2-step-1-sound-design--kits)
   - [Procedural Drum Synthesis (`gen!`)](#procedural-drum-synthesis-gen)
   - [Synthesizer Voice Architecture](#synthesizer-voice-architecture)
   - [Voice Stacking, Detune & Slaving (`>`, `G`, `H`, `N`)](#voice-stacking-detune--slaving--g-h-n)
   - [Human-Readable Voice Labels (`vt`)](#human-readable-voice-labels-vt)
3. [Step 2: Tempo-Synced FX Buses & Delays](#3-step-2-tempo-synced-fx-buses--delays)
4. [Step 3: Melodic Note Streams (`n>d /SS`)](#4-step-3-melodic-note-streams-nd-ss)
   - [Note Notation, Rests, Ties, and Slurs](#note-notation-rests-ties-and-slurs)
   - [Dynamic Step Streaming (`i&<stream>`)](#dynamic-step-streaming-istream)
5. [Step 4: Building Sequencer Patterns](#5-step-4-building-sequencer-patterns)
   - [Pattern Setup & Explicit Length (`yl`)](#pattern-setup--explicit-length-yl)
   - [Pattern Step Programming (`x<step>`)](#pattern-step-programming-xstep)
   - [Pattern Labels for Human Readability (`yt`)](#pattern-labels-for-human-readability-yt)
   - [Pattern Cloning for Fills & Variations (`ycp`)](#pattern-cloning-for-fills--variations-ycp)
   - [The Downbeat Conductor Pattern](#the-downbeat-conductor-pattern)
6. [Step 5: Scene Matrix & Song Arranger](#6-step-5-scene-matrix--song-arranger)
   - [Defining 8-Lane Scenes (`scene_define`)](#defining-8-lane-scenes-scene_define)
   - [Chaining Automated Progressions (`scene_chain`)](#chaining-automated-progressions-scene_chain)
   - [The Musical Outro & Auto-Stop](#the-musical-outro--auto-stop)
7. [Step 6: Live Performance Macros & Effects](#7-step-6-live-performance-macros--effects)
   - [Zero-Allocation Realtime Effects (`e>` / `e!`)](#zero-allocation-realtime-effects-e--e)
   - [Performance Aliases & User Macros (`[...] : ... ;`)](#performance-aliases--user-macros---)
8. [Live Jamming Playbook](#8-live-jamming-playbook)
9. [Quick Reference & Command Cheat Sheet](#9-quick-reference--command-cheat-sheet)

---

## 1. Core Concepts & Architecture

In **sk3**, music composition is structured as a hierarchical pipeline that connects raw DSP oscillators, wavetables, physical models, and pattern sequencers into an automated scene arranger:

```text
+-------------------------------------------------------------------------+
|                               NOTE STREAMS                              |
|           [ C2 C3 C2 C3  Eb2 Eb3 Eb2 -  Bb1 Bb2 Bb1 - ] n>d /SS 0        |
+-------------------------------------------------------------------------+
                                     |
                                     | i&0 (stream indexer)
                                     v
+-------------------------------------------------------------------------+
|                            SEQUENCER PATTERNS                           |
|   y0 [Kick 4-on-Floor] yt   y1 [Snare & Claps] yt   y3 [Moroder Bass] yt|
|   x0,4,8,12 [v0 l1]         x4,12 [v1 l.5]          x0..15 [v6 i&0]     |
+-------------------------------------------------------------------------+
                                     |
                                     | 8-lane pattern assignment
                                     v
+-------------------------------------------------------------------------+
|                               SCENE MATRIX                              |
|   Scene 0: Kick & Snare      -> ( 0  1 -1 -1 -1 -1 -1 127 )             |
|   Scene 1: Add Closed Hats   -> ( 0  1  2 -1 -1 -1 -1 127 )             |
|   Scene 2: Add Hypnotic Bass -> ( 0  1  2  3 -1 -1 -1 127 )             |
|   Scene 4: Anthem Melody     -> ( 0  1  2  3  6  7  9 127 )             |
|   Scene 7: Outro Breakdown   -> ( 0  1  2 -1 -1 -1 -1 127 )             |
+-------------------------------------------------------------------------+
                                     |
                                     | scene_chain (automated progression)
                                     v
+-------------------------------------------------------------------------+
|                             SCENE CHANGER                               |
|   0 4 1 scene_chain (4 loops -> Scene 1)                                |
|   ...                                                                   |
|   7 4 -1 scene_chain (4 loops -> STOP: clean musical outro!)            |
+-------------------------------------------------------------------------+
                                     |
                                     | voice triggering & DSP audio routing
                                     v
+-------------------------------------------------------------------------+
|                         VOICES & FX BUS ENGINE                          |
|   v0..v5  : Munich Drum Kit (Wavetables 100..104 via gen!)              |
|   v6, v7  : Layered PolyBLEP Saw & Pulse PWM Bass + Moog 24dB Ladder    |
|   v11     : 16th Shimmer Arp                                            |
|   v13, v14: Detuned Singing Donna Summer Lead + Portamento (g.075)      |
|   Bus 1   : 16th Ping-Pong Gallop Delay (BPM-locked)                    |
|   Bus 2   : Dotted-Eighth Floating Space Echo                           |
+-------------------------------------------------------------------------+
```

---

## 2. Step 1: Sound Design & Kits

### Procedural Drum Synthesis (`gen!`)
Synthesize analog circuits directly into wavetable slots `100..127` using procedural physical modeling:

```skode
# Slot 100: Punchy 909-style disco kick with warm sub drop & beater click
[drum:kick p:155 end:45 d:0.36 c:0.35 drive:0.2] 100 gen!
[Kick 909] wt 100

# Slot 101: Resonant electro snare with dual body modes & crisp wire snap
[drum:snare snap:0.75 d:0.07 tail:0.18 f:2900] 101 gen!
[Snare 808] wt 101

# Slot 102: Crisp closed hi-hat (6-oscillator inharmonic cluster)
[drum:hat open:0 d:0.045] 102 gen!
[Closed Hat] wt 102

# Slot 103: Sizzling open hi-hat
[drum:hat open:1 d:0.42] 103 gen!
[Open Hat] wt 103

# Slot 104: Stereo disco multi-burst handclap
[drum:clap bursts:3 spread:11 tail:0.22 f:1100] 104 gen!
[Disco Clap] wt 104
```

> [!TIP]
> Keep `drive:0.2` or below for pristine headroom. High drive values (e.g. `drive:1.5`) create aggressive distortion that can eat up mix headroom.

### Synthesizer Voice Architecture
Assign waveforms, envelopes, and filter settings to individual voices:
- **`w<type>`**: Waveform (`w0`=Sine, `w1`=PolyBLEP Pulse, `w2`=PolyBLEP Saw Up, `w3`=Saw Down, `w4`=Triangle, `w100+`=Wavetable slot).
- **`a<dB>`**: Voice amplitude in dBFS (e.g. `a-3.5`). Always use negative values for clean headroom.
- **`p<pan>`**: Stereo panning from `-1.0` (hard left) to `+1.0` (hard right).
- **`t<A,D,S,R>`**: Amplitude ADSR envelope in seconds.
- **`j<mode[,char]>`**: Multimode filter (`j1`=2-pole lowpass, `j1,1`=4-pole 24dB Moog ladder lowpass).
- **`k<cutoff_hz>`**: Filter cutoff frequency in Hz.
- **`q<resonance>`**: Filter resonance / Q factor.
- **`g<time>`**: Portamento / pitch glide time in seconds (e.g. `g.075`).

### Voice Stacking, Detune & Slaving (`>`, `G`, `H`, `N`)
Layer two voices together for thick analog leads and basslines:
```skode
# Voice 13: Lead voice
v13 w2 a-14.0 p.15 t.04,.35,.6,.25 g.075 j1,1 k1500 q1.0 r2 ds.40
[Donna Lead Main] vt

# Clone voice 13 parameters into voice 14 using '>'
v13 >14

# Customize voice 14: detune by 4 cents (N0,4), triangle wave (w4), pan left (p-.15)
v14 w4 a-16.5 p-.15 t.04,.35,.6,.25 g.075 j1 k1300 q0.8 N0,4 r2 ds.30
[Donna Lead Detune] vt

# Slave voice 14's gate (G) and velocity (H) to voice 13
v13 G14 H14
```

### Human-Readable Voice Labels (`vt`)
Tag every voice with its musical name so queries (`?v`, `v0`, etc.) display descriptive names:
```skode
[Physical Kick] vt
[Physical Snare] vt
[Moroder Saw Bass] vt
[Shimmer Arp] vt
```

---

## 3. Step 2: Tempo-Synced FX Buses & Delays

Configure delay lines locked to your session tempo (`M <bpm> <subdiv>`):
```skode
M 126 16    # 126 BPM, 16 steps per bar

# Bus 1: Iconic 16th Ping-Pong Gallop Delay (Dedicated to Bass & Arp)
DS 1 126 .25       # Bus 1, 126 BPM, 16th-note subdivision (0.25 beats)
DL 1 - - 5 2 2 8   # Left/right time, feedback, and damping
DD 1 5 3           # Delay character / modulation depth
DP 1 1             # Stereo ping-pong mode enabled

# Bus 2: Dotted-Eighth Floating Space Echo (Dedicated to Lead Melody)
DS 2 126 .75       # Bus 2, 126 BPM, dotted-eighth subdivision (0.75 beats)
DL 2 - - 6 3 3 8
DD 2 6 3
DP 2 1
```

Route voices into buses using `r<bus> ds<send_level>`:
```skode
v6 r1 ds.30     # Bass sent to Bus 1 gallop delay at 30%
v11 r1 ds.35    # Arp sent to Bus 1 gallop delay at 35%
v13 r2 ds.40    # Lead sent to Bus 2 space echo at 40%
```

---

## 4. Step 3: Melodic Note Streams (`n>d /SS`)

### Note Notation, Rests, Ties, and Slurs
Write musical phrases using standard note names (`C2`, `Eb3`, `F#4`) and convert them into stream buffers using `n>d /SS <id>`:
- **`Note`**: Triggers a note attack.
- **`.` (Tie)**: Sustains the previous pitch without re-triggering the envelope attack.
- **`-` (Rest)**: Releases the note envelope to introduce rhythmic space.
- **`~Note` (Slur)**: Legato transition to target pitch.

```skode
# Stream 0: The Hypnotic 4-Bar Octave Bass (C -> Eb -> Bb -> F, 64 steps)
[
  # Bar 1: C octave pulse
  C2 C3 C2 C3  C2 C3 C2 C3  C2 C3 C2 C3  C2 C3 C2 -
  # Bar 2: Eb octave pulse
  Eb2 Eb3 Eb2 Eb3  Eb2 Eb3 Eb2 Eb3  Eb2 Eb3 Eb2 Eb3  Eb2 Eb3 Eb2 -
  # Bar 3: Bb octave pulse
  Bb1 Bb2 Bb1 Bb2  Bb1 Bb2 Bb1 Bb2  Bb1 Bb2 Bb1 Bb2  Bb1 Bb2 Bb1 -
  # Bar 4: F octave pulse
  F2 F3 F2 F3  F2 F3 F2 F3  F2 F3 F2 F3  F2 F3 F2 -
] n>d /SS 0
```

### Dynamic Step Streaming (`i&<stream>`)
To play a stream across a 16-step pattern, bind the voice step to `i&<id>`:
```skode
y3 %1
yl 16
[v6 i&0] x0,1,2,3,4,5,6,7
[v6 i&0] x8,9,10,11,12,13,14,15
```
Each time the pattern advances, `i&0` pulls the next note from Stream 0. Because Stream 0 has 64 notes and the pattern has 16 steps, the phrase cycles perfectly across 4 bars.

---

## 5. Step 4: Building Sequencer Patterns

### Pattern Setup & Explicit Length (`yl`)
Always set an explicit length (`yl 16`) and modulo (`%1` for 16th notes):
```skode
y0 %1
[Kick 4-on-Floor] yt
yl 16
[v0 l1 v5 g0 n48 g.06 n24 l1] x0,4,8,12
```

### Pattern Step Programming (`x<step>`)
Place events onto step indices `0..15`:
```skode
# Disco Snare on 2 and 4 (Steps 4 and 12)
y1 %1
[Snare & Claps] yt
yl 16
[v1 l.5] x4,12

# Sizzling 16th Closed Hats with Accents
y2 %1
[16th Closed Hats] yt
yl 16
[v2 l.38] x0,2,4,6,8,10,12,14     # Accented on downbeats
[v2 l.22] x1,3,5,7,9,11,13,15     # Softer in-between 16ths
```

### Pattern Labels for Human Readability (`yt`)
Set pattern labels using `[Label] yt`. When you inspect patterns with `M` or `Z?`, sk3 prints clear names:
```skode
y0 [Kick 4-on-Floor] yt
y1 [Snare & Claps] yt
y2 [16th Closed Hats] yt
y3 [Moroder Octave Bass] yt
y6 [16th Shimmer Arp] yt
y7 [Donna Anthem Lead] yt
y127 [Arranger Conductor] yt
```

### Pattern Cloning for Fills & Variations (`ycp`)
Clone existing patterns in one command and edit just the fill steps:
```skode
# Clone Pattern 1 (Snare) into Pattern 5 for a climax snare roll
1 5 ycp
5 y
[Snare Roll Fill] yt
[v1 l.5] x4,12
[v1 l.35] x13,14,15   # 16th-note roll into the downbeat
```

### The Downbeat Conductor Pattern
Designate pattern 127 as the master downbeat reference:
```skode
y127 %1
[Arranger Conductor] yt
yl 16
[v0] x15 yp127
```

---

## 6. Step 5: Scene Matrix & Song Arranger

### Defining 8-Lane Scenes (`scene_define`)
A **scene** defines which patterns are active across the 8 mixer lanes:
`Lane 0:Kick  Lane 1:Snare  Lane 2:ClosedHat  Lane 3:Bass  Lane 4:Arp  Lane 5:Lead  Lane 6:OpenHat/Clap  Lane 7:Conductor`

Pass an 8-integer array followed by `scene_define <scene_id>`:
```skode
# Scene 0: Foundation (Kick + Snare)
( 0  1 -1 -1 -1 -1 -1 127 ) scene_define 0

# Scene 1: Add Closed Hats
( 0  1  2 -1 -1 -1 -1 127 ) scene_define 1

# Scene 2: Add Hypnotic Octave Bass
( 0  1  2  3 -1 -1 -1 127 ) scene_define 2

# Scene 3: Add Offbeat Open Hat Sizzle
( 0  1  2  3 -1 -1  9 127 ) scene_define 3

# Scene 4: Full Anthem (Lead + Shimmer Arp)
( 0  1  2  3  6  7  9 127 ) scene_define 4

# Scene 5: Climax (Syncopated Kicks + Snare Rolls + Claps)
( 4  5  2  3  6  7 10 127 ) scene_define 5

# Scene 6: Breakdown (Bass & Drums only)
( 0  1  2  3 -1 -1  9 127 ) scene_define 6

# Scene 7: Outro (Deconstruct to Kick, Snare & Hats)
( 0  1  2 -1 -1 -1 -1 127 ) scene_define 7
```

### Chaining Automated Progressions (`scene_chain`)
Automate song progression using `scene_id repeats next_scene scene_chain`:
```skode
0 4 1 scene_chain    # Scene 0 plays 4 loops, then switches to Scene 1
1 4 2 scene_chain    # Scene 1 plays 4 loops, then switches to Scene 2
2 4 3 scene_chain    # Scene 2 plays 4 loops, then switches to Scene 3
3 4 4 scene_chain    # Scene 3 plays 4 loops, then switches to Scene 4
4 8 5 scene_chain    # Scene 4 plays 8 loops, then switches to Scene 5
5 4 6 scene_chain    # Scene 5 plays 4 loops, then switches to Scene 6
6 4 7 scene_chain    # Scene 6 plays 4 loops, then switches to Scene 7
```

### The Musical Outro & Auto-Stop
To create a natural song ending rather than looping forever:
```skode
7 4 -1 scene_chain   # Scene 7 plays 4 loops, then cleanly STOPS playback!
```
When `next_scene` is `-1` and `repeats > 0`:
1. The arranger counts the 4 bars of the outro groove.
2. On the final downbeat, it halts pattern scheduling.
3. Sounding notes decay naturally and delay tails fade smoothly into silence without pops or clicks.

---

## 7. Step 6: Live Performance Macros & Effects

### Zero-Allocation Realtime Effects (`e>` / `e!`)
Precompile realtime effect mutations into slots `0..5` for instantaneous execution:
```skode
[v0 a0.0 n36] e> 0   # Slot 0: Accent punch kick on downbeat
[v6 k180]     e> 1   # Slot 1: Bass filter drop (underwater breakdown)
[v6 k580]     e> 2   # Slot 2: Bright open bass filter sweep
[v6 k300]     e> 3   # Slot 3: Reset bass filter to standard punch
[v13 ds.60]   e> 4   # Slot 4: Dub delay wash on vocals
[v13 ds.40]   e> 5   # Slot 5: Reset vocal delay send
```
Trigger any effect live with `0 e!`, `1 e!`, etc.

### Performance Aliases & User Macros (`[...] : ... ;`)
Wrap complex commands into short, human-friendly names:
```skode
# Navigation macros (quantized to the next downbeat)
[scene_kick_snare] : 0 scene_cue ;
[scene_melody]     : 4 scene_cue ;
[scene_climax]     : 5 scene_cue ;
[outro]            : 7 scene_cue ;

# Effect aliases
[fx_muffle] : 1 e! ;
[fx_open]   : 2 e! ;
[fx_reset]  : 3 e! ;
[fx_wash]   : 4 e! ;
[fx_clean]  : 5 e! ;

# Arranger mode toggles
[loop_disco] : 7 4 0 scene_chain ;    # Loop indefinitely on the outro
[end_disco]  : 7 4 -1 scene_chain ;   # Auto-stop after 4 bars of outro

# Transport controls
[play] : Z0 Z! scene_now 0 ;          # Clean start from Scene 0
```

---

## 8. Live Jamming Playbook

### Starting the Set
Type:
```skode
play
```
This rewinds the timeline (`Z0 Z!`) and launches Scene 0. The song will automatically build and progress through all 8 scenes.

### Jumping Scenes on the Fly
During performance, cue any scene quantized to the next bar's downbeat:
```skode
scene_melody     # or: 4 scene_cue
```
The current bar will complete, and on the "1", the new scene seamlessly takes over.

### Live Filter Sweeps & Dub Drops
- **Breakdown**: Type `1 e!` (or `fx_muffle`) to choke the bass cutoff down to 180 Hz.
- **Build-up**: Type `2 e!` (or `fx_open`) to open the Moog ladder filter up to 580 Hz for sizzling energy.
- **Drop**: On the downbeat, type `3 e!` (or `fx_reset`) to snap the bass back into punchy drive.
- **Dub Wash**: Type `4 e!` to throw the soaring lead voice into deep feedback delay space.

### Ending the Song Smoothly
Whenever you want to wrap up the jam:
```skode
outro
```
At the next downbeat, sk3 drops into the 4-bar deconstructed groove (kick, snare, closed hat), plays out the 4 bars, and halts the transport automatically.

---

## 9. Quick Reference & Command Cheat Sheet

| Command | Category | Description | Example |
| :--- | :--- | :--- | :--- |
| `M <bpm> <div_per_bar>` | Sequencer | Sets session tempo and subdivisions per bar | `M 126 16` |
| `V <dB>` | Master | Sets master output gain in dBFS | `V -3.0` |
| `v<voice>` | Voice | Selects active voice | `v6` |
| `[Name] vt` | Voice | Assigns human-readable voice label | `[Moroder Bass] vt` |
| `w<type>` | Oscillator | Selects waveform (0:sine, 1:pulse, 2:saw, 100+:wavetable) | `w2` |
| `a<dB>` | Voice | Sets voice amplitude in dBFS | `a-4.5` |
| `p<pan>` | Voice | Sets stereo pan (-1.0 to 1.0) | `p.15` |
| `t <A,D,S,R>` | Envelope | Sets ADSR envelope times in seconds | `t .04 .35 .6 .25` |
| `g <time>` | Portamento | Sets pitch glide time in seconds (0 = disabled) | `g .075` |
| `j <mode[,char]>` | Filter | Sets filter type (1:2-pole LP, 1,1:4-pole Moog ladder) | `j1,1` |
| `k <hz>` | Filter | Sets filter cutoff frequency in Hz | `k1500` |
| `q <resonance>` | Filter | Sets filter resonance Q | `q2.2` |
| `r<bus> ds<amt>` | Delay | Sets delay send bus and amount | `r1 ds.35` |
| `v<a> ><b>` | Voice | Clones voice `a` configuration into voice `b` | `v13 >14` |
| `v<a> G<b>` | Voice | Slaves voice `b` gate / note trigger to voice `a` | `v13 G14` |
| `v<a> H<b>` | Voice | Slaves voice `b` velocity to voice `a` | `v13 H14` |
| `N <semi,cents>` | Pitch | Detunes voice by semitones and cents | `N0,4` |
| `[drum:...] <slot> gen!` | DSP | Procedural physical modeling drum synthesis | `[drum:kick ...] 100 gen!` |
| `[notes] n>d /SS <id>` | Stream | Compiles note string into note stream `<id>` | `[C2 C3 . C2] n>d /SS 0` |
| `y<pattern> %<mod>` | Sequencer | Selects pattern and clock step modulo | `y0 %1` |
| `[Label] yt` | Sequencer | Assigns human-readable pattern label | `[Kick 4-on-Floor] yt` |
| `yl <length>` | Sequencer | Sets explicit pattern step length | `yl 16` |
| `[cmds] x<steps>` | Sequencer | Programs steps in active pattern | `[v0 l1] x0,4,8,12` |
| `[v<n> i&<id>] x<steps>` | Sequencer | Streams notes from Stream `<id>` on specified steps | `[v6 i&0] x0..15` |
| `<src> <dst> ycp` | Sequencer | Clones pattern `<src>` to `<dst>` | `1 5 ycp` |
| `(<p0>..<p7>) scene_define <id>` | Arranger | Defines 8-lane pattern assignment for scene `<id>` | `(0 1 2 3 -1 -1 -1 127) scene_define 2` |
| `<s> <repeats> <next> scene_chain` | Arranger | Chains scene `<s>` to `<next>` after `<repeats>` loops | `0 4 1 scene_chain` |
| `<s> <repeats> -1 scene_chain` | Arranger | Chains scene `<s>` to auto-stop after `<repeats>` loops | `7 4 -1 scene_chain` |
| `<id> scene_cue` | Arranger | Cues scene `<id>` at next downbeat | `4 scene_cue` |
| `<id> scene_now` | Arranger | Immediately switches to scene `<id>` | `0 scene_now` |
| `scene_active` | Arranger | Prints active scene index | `scene_active` |
| `[cmds] e> <slot>` | Effects | Precompiles realtime effect macro into slot `0..5` | `[v6 k180] e> 1` |
| `<slot> e!` | Effects | Immediately triggers precompiled effect `<slot>` | `1 e!` |
| `Z?` / `M` | Diagnostics | Displays global sequencer status and labeled patterns | `M` |
| `stop` | Transport | Silently stops transport, halts scene arranger & mutes voices | `stop` |
