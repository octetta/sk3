# Inspirations & Lineage

The architecture, sound design philosophy, and expressive control paradigms in `sk3` and the **Skode** language are deeply rooted in the history of electronic music, pioneering synthesizer hardware, and real-time audio programming languages.

This document traces the direct technical lineages and visionary pioneers whose work inspired `sk3`. For the chronological evolution across earlier prototypes and projects (`amy-not-the-samey`, `rototem`, `rattle`, `hlokk`, `exsynthia`, `skred`, `pulp`), see [HISTORY.md](HISTORY.md).

---

## 1. ASCII Wire Protocol: The AMY Project

- **Project**: [AMY (Additive Music synthesizer librarY)](https://github.com/shorepine/amy)
- **Creators**: [Brian Whitman](https://github.com/bwhitman) & [Dan Ellis](https://github.com/dpwe) / [Shore Pine Sound Systems](https://shorepine.github.io/)

The wire control syntax of **ANDS** and **SKODE** in `sk3` drew direct inspiration from AMY's brilliant ASCII wire protocol. AMY proved that complex, multi-timbral polyphonic synthesizer engines do not require bloated binary protocols or heavy JSON overhead: they can be driven at ultra-low latency with compact, single-character parameter commands (e.g., `v0f440a0.5l1Z`) transmitted over serial pipes, POSIX IPC, UDP datagrams, or WebAssembly memory buffers.

Joseph Stewart was an early contributor to AMY, submitting the migration that replaced PortAudio with `miniaudio` for desktop platforms—a lightweight, single-file audio backend choice that proved extraordinarily reliable and carried forward directly into `skred` and `sk3`.

`sk3` builds upon this foundation by extending the ASCII wire stream into a full Forth-inspired concatenative domain-specific language with macros, array parameters, and deterministic schedule queues.

---

## 2. Wavetable Synthesis & Digital Delay: Korg DW-8000 & DWGS

- **Institution**: [Korg Corporation](https://www.korg.com/)
- **Technology**: [DWGS (Digital Waveform Generator System) & Onboard Digital Delay](https://en.wikipedia.org/wiki/Korg_DW-8000)

Introduced in 1985 with the **Korg DW-8000** and **DW-6000**, DWGS established the hybrid synthesis era by pairing sampled single-cycle digital waveforms stored in ROM with analog resonant filters and envelope shapers. Rather than relying solely on pure mathematical primitives (sine, saw, pulse), DWGS provided harmonically dense, complex acoustic and hybrid spectra.

The `dwg` wavetable recipes in `sk3` (`waves/dwg/` and internal oscillator configurations `w15..w30`) pay direct homage to the Korg DWGS concept, delivering instant digital waveguide-inspired string, clarinet, clavi, piano, and brass timbres through a unified oscillator architecture.

### Direct Influence: The DW-8000 Onboard Digital Delay
The Korg DW-8000 was pioneering not only for its wavetables, but for incorporating one of the earliest onboard digital delay units in a commercial polyphonic synthesizer. The DW-8000 delay line was beloved for its warm, characterful chorus, flanging, doubling, and slapback echoes, parameterized by discrete coarse time ranges, fine adjustments, feedback amounts, and dedicated LFO modulation.

The stereo delay line architecture in `pulp` and `sk3` ([src/synth-delay.c](file:///home/stewartj/book/sk3/src/synth-delay.c)) was **directly influenced and modeled after the Korg DW-8000 delay unit**:
- **Discrete Coarse & Fine Time Scaling**: The delay time calculation (`delay_base_ms`) derives base delay times via binary shifts scaled by fine fractions ($8.0 \times 2^{\text{coarse}} \times (0.5 + 0.5 \cdot \text{fine}/15)$), exactly reflecting the DW-8000 parameter hierarchy.
- **Dedicated Delay Modulation**: Discrete modulation frequency ($0..31$) and depth ($0..31$) LFO parameters driving sub-sample delay time modulation for rich chorus and flanging effects.
- **Grit & Bit-Depth Quantization**: Optional bit reduction (`delay_grit`) replicating the vintage conversion character and harmonic grit of early digital delay lines.
- **Track-Based Routing**: Multi-bus delay sends (`ds`) allowing voices to route individual amounts into dedicated delay stages.

---

## 3. Custom Silicon & The 70s/80s "POKE" Live Coding Era: Bob Yannes (SID) & Doug Neubauer (POKEY)

- **Visionaries**:
  - [Bob Yannes](https://en.wikipedia.org/wiki/Bob_Yannes) (MOS Technology / Ensoniq)
  - [Doug Neubauer](https://en.wikipedia.org/wiki/Doug_Neubauer) (Atari)
- **Custom Silicon & Iconic Machines**:
  - [MOS Technology 6581 SID (Sound Interface Device)](https://en.wikipedia.org/wiki/MOS_Technology_6581) (Commodore 64, 1982)
  - [Atari POKEY (POt Keyboard Integrated Circuit)](https://en.wikipedia.org/wiki/POKEY) (Atari 400 / 800, 1979; Atari 5200; arcade classics: *Asteroids Deluxe*, *Missile Command*, *Tempest*)
  - [Ensoniq 5503 DOC (Digital Oscillator Chip)](https://en.wikipedia.org/wiki/Ensoniq_ESQ-1) (Ensoniq Mirage, 1984; ESQ-1, 1986; SQ-80, 1987)
  - [IEEE Spectrum: The Design of the SID Chip](https://spectrum.ieee.org/design-of-the-sid-chip)

### The 70s/80s "POKE" Live Coding Heritage
Long before modern "live coding" became a formalized computer music discipline, the purest form of interactive sound synthesis was happening in bedrooms and computer clubs in the late 1970s and early 1980s. Sitting at an Atari 400/800 or a Commodore 64, typing direct `POKE` commands in BASIC—such as `POKE 53760, ...` to modulate POKEY's 4-channel polynomial distortion counters, or `POKE 54272, ...` to manipulate SID's oscillators, PWM, and multi-mode filters—provided immediate, visceral acoustic feedback. 

There was no compilation step, no asset loading, and no abstraction layer: typing a value into a memory-mapped hardware address instantly changed the physical vibration of the speaker cone. It was spontaneous, direct, and electric—the original 8-bit live coding.

### Technological Lineage in `sk3`
The interaction model of `sk3` and the **Skode** language is a direct spiritual descendant of this "POKE" ethos:
- **Direct Register Manipulation**: Typing atoms into the `sk3` REPL (`v0 w1 440 f 1 l`, `c 1 0.5`, `ds 0.8`) is modern, high-level "POKEing"—writing values straight into synthesizer voice registers with zero delay and instantaneous audible transformation.
- **`waves/esq1/`**: The complete authentic collection of Ensoniq ESQ-1 8-bit digital wavetables (`50_saw` through `81_oct5th`) built into the `sk3` synthesis engine.
- **SID Waveform Emulation in `sid-to-skode`**: Vector synthesis patch formulas developed in `sid-to-skode` (`w300` triangle, `w301` saw, `w302` pulse, `w303` saw+tri, `w304` saw+pulse) running atop `sk3`'s embedded `k-synth` engine.
- **POKEY-Inspired Poly-Counters**: Distortion modes, noise registers, and raw frequency dividing concepts inspired by Doug Neubauer's ingenious 4-channel POKEY architecture.

---

## 4. Phase Distortion & Wavetable Pulse-Width Simulation: Casio CZ-101

- **Institution**: [Casio Computer Co., Ltd.](https://en.wikipedia.org/wiki/Casio)
- **Architect & Instrument**: [Casio CZ-101 (1984)](https://en.wikipedia.org/wiki/Casio_CZ-101) | [Phase Distortion Synthesis](https://en.wikipedia.org/wiki/Phase_distortion_synthesis)

In 1984, Casio introduced the CZ-101 powered by **Phase Distortion (PD)** synthesis. Instead of filtering a harmonically rich waveform with expensive analog filters, Casio distorted the phase angle reading of a stored lookup table at non-linear rates, dynamically warping a basic cosine table into sawtooth, pulse, double-sine, and resonant waveforms via digital envelopes.

The phase distortion engine in `sk3` and `pulp` (`cz_phasor` in [src/synth-osc.c](file:///home/stewartj/book/sk3/src/synth-osc.c) and opcode `c`) was directly inspired by the CZ-101, combined with a crucial creative breakthrough: **the realization that CZ phase distortion methods make pulse-width simulation possible on arbitrary single-cycle wavetables**.

In classical wavetable synthesis, altering the pulse width or duty cycle of a fixed single-cycle waveform normally requires either storing large multi-frame wavetables or computing expensive real-time FFT resynthesis. By applying piecewise-linear CZ phase warping (`saw -> pulse`, `folded sine`, etc.) to the normalized phase phasor *prior* to indexing the wavetable, `sk3` can continuously modulate the duty cycle and harmonic asymmetry of *any* single-cycle wavetable (including Korg DWGS, Ensoniq ESQ-1, and custom user tables) in real time with near-zero CPU overhead.

---

## 5. Voltage Control & Subtractive Synthesis: Dr. Robert Moog

- **Pioneer**: [Dr. Robert A. Moog](https://en.wikipedia.org/wiki/Robert_Moog)
- **Organizations**: [Moog Music](https://www.moogmusic.com/) | [Bob Moog Foundation](https://moogfoundation.org/)

Bob Moog invented the voltage-controlled analog modular synthesizer, defining the foundational architecture of electronic sound synthesis: oscillators, voltage-controlled amplifiers (VCA), the iconic 24 dB/octave resonant transistor ladder filter (VCF), and ADSR envelope generation.

`sk3`'s per-voice signal flow—routing dual multi-algorithm oscillators through resonant state variable filters and dedicated exponential amplitude/timbre envelopes—is an evolution of the subtractive synthesis paradigm pioneered by Bob Moog.

---

## 6. Expressive Virtuosity & Acoustics: Wendy Carlos

- **Pioneer**: [Wendy Carlos](https://www.wendycarlos.com/)
- **Seminal Works**: *Switched-On Bach* (1968), *Sonic Seasonings* (1972), *A Clockwork Orange* (1971), *The Shining* (1980), *Tron* (1982), *Beauty in the Beast* (1986)

Wendy Carlos brought electronic synthesis to the forefront of modern music. Her rigorous sonic craftsmanship proved that synthetic instruments could convey the highest levels of human touch, dynamic nuance, and polyphonic phrasing. Carlos worked closely with Bob Moog to refine touch-sensitive keyboards, custom envelope shaping, and portamento dynamics. Her later masterpieces, such as *Beauty in the Beast*, broke ground in microtonal tunings, alternative temperaments, and digital acoustic modeling.

The design of `sk3`—emphasizing musical phrasing, sub-millisecond parameter modulation, and expressive micro-timing in live performance—is profoundly inspired by Wendy Carlos's acoustic artistry.

---

## 7. Digital Synthesis & The Universal Protocol: Dave Smith

- **Pioneer**: [Dave Smith](https://www.sequential.com/dave-smith/)
- **Organization**: [Sequential](https://www.sequential.com/)
- **Universal Standard**: [The MIDI Association](https://www.midi.org/)

Dave Smith revolutionized music technology twice over. In 1978, he founded Sequential Circuits and created the **Prophet-5**, the first commercially successful polyphonic synthesizer with microprocessor patch storage and dynamic voice assignment. In 1982–1983, Dave Smith authored the proposal for and co-created **MIDI (Musical Instrument Digital Interface)**, the universal open standard that unified musical hardware worldwide. Later, he pioneered software synthesis with the Seer Systems Reality engine.

`sk3`'s dynamic polyphonic voice pooling, group allocation, and first-class hardware MIDI integration (`src/midi.c` and `src/vendor/minimidio/`) stand directly upon Dave Smith's enduring standards.

---

## 8. Strongly-Timed Live Audio Coding: Ge Wang (ChucK)

- **Pioneer**: [Ge Wang](https://ccrma.stanford.edu/~gow/)
- **Institution**: [Stanford University CCRMA (Center for Computer Research in Music and Acoustics)](https://ccrma.stanford.edu/)
- **Language**: [ChucK: Strongly-Timed Concurrent Audio Programming Language](https://chuck.stanford.edu/)
- **Book**: *The Art of Virtual Acoustic Design*

Ge Wang introduced the concept of **strongly-timed audio programming** through ChucK. In ChucK, time is not an implicit side-effect of audio buffers; time is an explicit, first-class language construct (`100::ms => now`). This paradigm enabled concurrent "shreds" to synchronize sample-accurately and opened new horizons for live coding and on-the-fly computer music performance.

`sk3` and the Skode runtime adopt this deterministic approach to time: scheduled events, micro-delays, and pattern ticks execute deterministically with explicit clock control, bridging live typing with sample-accurate synthesis.

---

## 9. Real-Time Dataflow & Computer Music: Miller Puckette (Max & Pure Data)

- **Pioneer**: [Miller Puckette](https://msp.ucsd.edu/)
- **Institutions**: [IRCAM](https://www.ircam.fr/) | [UC San Diego Department of Music](https://music.ucsd.edu/)
- **Environments**: [Pure Data (Pd)](https://puredata.info/) | [Max (Cycling '74)](https://cycling74.com/)
- **Textbook**: [*The Theory and Technique of Electronic Music*](https://msp.ucsd.edu/techniques.htm)

Miller Puckette designed **Max** at IRCAM in the mid-1980s and subsequently developed **Pure Data (Pd)** in the 1990s as an open-source real-time graphical programming environment for audio, video, and graphical processing. His dual-rate architecture—distinguishing high-rate audio signal vectors (`~`) from asynchronous control message processing—has formed the theoretical backbone of modern digital audio software.

`sk3` honors Miller Puckette's principles through its strict separation of high-priority audio render callbacks from non-blocking lockless control event dispatchers (`src/control-events.h`, `src/skqueue.c`), ensuring audio callback execution remains realtime-safe and glitch-free.

---

## 10. Array Programming & Holistic Vector Audio: Arthur Whitney (K / ksimple)

- **Pioneer**: [Arthur Whitney](https://en.wikipedia.org/wiki/Arthur_Whitney_(computer_scientist))
- **Lineage**: [ksimple (kparc)](https://github.com/kparc/ksimple) | [Kx Systems](https://kx.com/) | [A+ / K / Q](https://en.wikipedia.org/wiki/K_(programming_language))

Arthur Whitney transformed data processing by treating entire collections of values as primitive, first-class entities manipulated by terse, expressive mathematical verbs. His tiny `ksimple` interpreter proved that an astonishingly powerful, array-oriented computational engine could fit within a few hundred lines of elegant, uncompromising C code.

`k-synth`—vendored inside `sk3` at `src/vendor/ksynth/`—draws directly from Arthur Whitney's `ksimple` lineage. Instead of viewing audio as a continuous sequential stream processed one sample loop at a time, `k-synth` treats sound as a holistic mathematical vector. A kick drum, an envelope ramp, or a bandlimited wavetable is simply an array:
- `A: !8000` (generate an index ramp of 8,000 samples)
- `E: 1 - 0.000125 * A` (vectorized linear decay)
- `K: 5 * (s 0.007 * A) * E * E` (vectorized sine oscillator multiplied by squared exponential envelope)

By embedding this array-language engine, `sk3` inherits the lightning-fast vector mathematics and radical economy of expression pioneered by Arthur Whitney.

---

## 11. Concatenative Minimalism & Dictionary Execution: Chuck Moore (Forth)

- **Pioneer**: [Charles H. "Chuck" Moore](https://en.wikipedia.org/wiki/Charles_H._Moore)
- **Creations**: [Forth (1970)](https://www.forth.com/about-forth/) | [ColorForth](https://www.colorforth.com/) | [GreenArrays](http://www.greenarrays.com/)
- **Philosophy**: *"Keep it simple. As simple as you can. Do not solve problems you don't have."*

Chuck Moore invented the Forth programming language in 1970, championing an uncompromising philosophy of extreme simplicity, zero-overhead execution, and interactive live control. In Forth, there are no complex syntax trees, no heavy grammars, and no arbitrary syntactic ceremony: execution proceeds through a dictionary of words that operate on data through a concatenative flow.

The design of the **Skode** language and runtime in `sk3` is deeply indebted to Chuck Moore's Forth paradigms:
- **Dictionary-Driven Execution**: Words in Skode (`puts`, `wt`, `G`, `HS`, `play`, `stop`) are registered in a fast lookup dictionary (`src/skode-dict.c`) that directly dispatches native C functions.
- **Postfix & Flexible Argument Flow**: Arguments can precede or follow atoms naturally (`v0 w1 440 f 1 l` or `v0 w1 f440 l1`), mimicking Forth's postfix ergonomics while remaining optimized for real-time live typing.
- **Eliminating Syntax Overhead**: Skode scripts dispense with parentheses, commas, and complex grammar hierarchies, executing token-by-token directly into audio state changes.
- **Interactive REPL Philosophy**: Just as Forth systems were designed to boot instantaneously and allow engineers to interact with hardware live, `sk3` provides an immediate interactive environment where music and synthesis parameters are sculpted in real time.
