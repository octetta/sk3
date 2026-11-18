# Inspirations & Lineage

The architecture, sound design philosophy, and expressive control paradigms in `sk3` and the **Skode** language are deeply rooted in the history of electronic music, pioneering synthesizer hardware, and real-time audio programming languages.

This document traces the direct technical lineages and visionary pioneers whose work inspired `sk3`.

---

## 1. ASCII Wire Protocol: The AMY Project

- **Project**: [AMY (Additive Music synthesizer librarY)](https://github.com/shorepine/amy)
- **Creators**: [Brian Whitman](https://github.com/bwhitman) & [Dan Ellis](https://github.com/dpwe) / [Shore Pine Sound Systems](https://shorepine.github.io/)

The wire control syntax of **ANDS** and **SKODE** in `sk3` drew direct inspiration from AMY's brilliant ASCII wire protocol. AMY proved that complex, multi-timbral polyphonic synthesizer engines do not require bloated binary protocols or heavy JSON overhead: they can be driven at ultra-low latency with compact, single-character parameter commands (e.g., `v0f440a0.5l1Z`) transmitted over serial pipes, POSIX IPC, UDP datagrams, or WebAssembly memory buffers.

`sk3` builds upon this foundation by extending the ASCII wire stream into a full Forth-inspired concatenative domain-specific language with macros, array parameters, and deterministic schedule queues.

---

## 2. Wavetable Synthesis: Korg DW-8000 & DWGS

- **Institution**: [Korg Corporation](https://www.korg.com/)
- **Technology**: [DWGS (Digital Waveform Generator System)](https://en.wikipedia.org/wiki/Korg_DW-8000)

Introduced in 1985 with the **Korg DW-8000** and **DW-6000**, DWGS established the hybrid synthesis era by pairing sampled single-cycle digital waveforms stored in ROM with analog resonant filters and envelope shapers. Rather than relying solely on pure mathematical primitives (sine, saw, pulse), DWGS provided harmonically dense, complex acoustic and hybrid spectra.

The `dwg` wavetable recipes in `sk3` (`waves/dwg/` and internal oscillator configurations `w15..w30`) pay direct homage to the Korg DWGS concept, delivering instant digital waveguide-inspired string, clarinet, clavi, piano, and brass timbres through a unified oscillator architecture.

---

## 3. Custom Silicon & Digital Synthesis: Bob Yannes (SID & Ensoniq ESQ-1)

- **Visionary**: [Bob Yannes](https://en.wikipedia.org/wiki/Bob_Yannes)
- **Silicon & Synthesizers**:
  - [MOS Technology 6581 SID (Sound Interface Device)](https://en.wikipedia.org/wiki/MOS_Technology_6581) (Commodore 64, 1982)
  - [Ensoniq 5503 DOC (Digital Oscillator Chip)](https://en.wikipedia.org/wiki/Ensoniq_ESQ-1) (Ensoniq Mirage, 1984; ESQ-1, 1986; SQ-80, 1987)
  - [IEEE Spectrum: The Design of the SID Chip](https://spectrum.ieee.org/design-of-the-sid-chip)

Few engineers have shaped the sound of modern electronic music and digital synthesis as profoundly as Bob Yannes.
At MOS Technology, he single-handedly architected the 6581 SID chip, bringing multi-waveform oscillators, variable pulse-width modulation, ring modulation, hard sync, and analog multi-mode filtering to millions of Commodore 64 personal computers.

Yannes then co-founded **Ensoniq**, designing the 5503 Digital Oscillator Chip (DOC). The Ensoniq ESQ-1 took wavetable synthesis into uncharted creative territory by combining 32 wavetable oscillators with subtractive analog filters and multi-stage envelopes.

`sk3` honors Bob Yannes's contributions on multiple fronts:
- **`waves/esq1/`**: The complete authentic collection of Ensoniq ESQ-1 8-bit digital wavetables (`50_saw` through `81_oct5th`) built into the `sk3` synthesis engine.
- **Ksynth SID Emulation (`w300..w304`)**: Bandlimited SID triangle, sawtooth, pulse/PWM, and combined waveform models designed for chiptune and demo-scene authenticity.

---

## 4. Voltage Control & Subtractive Synthesis: Dr. Robert Moog

- **Pioneer**: [Dr. Robert A. Moog](https://en.wikipedia.org/wiki/Robert_Moog)
- **Organizations**: [Moog Music](https://www.moogmusic.com/) | [Bob Moog Foundation](https://moogfoundation.org/)

Bob Moog invented the voltage-controlled analog modular synthesizer, defining the foundational architecture of electronic sound synthesis: oscillators, voltage-controlled amplifiers (VCA), the iconic 24 dB/octave resonant transistor ladder filter (VCF), and ADSR envelope generation.

`sk3`'s per-voice signal flow—routing dual multi-algorithm oscillators through resonant state variable filters and dedicated exponential amplitude/timbre envelopes—is an evolution of the subtractive synthesis paradigm pioneered by Bob Moog.

---

## 5. Expressive Virtuosity & Acoustics: Wendy Carlos

- **Pioneer**: [Wendy Carlos](https://www.wendycarlos.com/)
- **Seminal Works**: *Switched-On Bach* (1968), *Sonic Seasonings* (1972), *A Clockwork Orange* (1971), *The Shining* (1980), *Tron* (1982), *Beauty in the Beast* (1986)

Wendy Carlos brought electronic synthesis to the forefront of modern music. Her rigorous sonic craftsmanship proved that synthetic instruments could convey the highest levels of human touch, dynamic nuance, and polyphonic phrasing. Carlos worked closely with Bob Moog to refine touch-sensitive keyboards, custom envelope shaping, and portamento dynamics. Her later masterpieces, such as *Beauty in the Beast*, broke ground in microtonal tunings, alternative temperaments, and digital acoustic modeling.

The design of `sk3`—emphasizing musical phrasing, sub-millisecond parameter modulation, and expressive micro-timing in live performance—is profoundly inspired by Wendy Carlos's acoustic artistry.

---

## 6. Digital Synthesis & The Universal Protocol: Dave Smith

- **Pioneer**: [Dave Smith](https://www.sequential.com/dave-smith/)
- **Organization**: [Sequential](https://www.sequential.com/)
- **Universal Standard**: [The MIDI Association](https://www.midi.org/)

Dave Smith revolutionized music technology twice over. In 1978, he founded Sequential Circuits and created the **Prophet-5**, the first commercially successful polyphonic synthesizer with microprocessor patch storage and dynamic voice assignment. In 1982–1983, Dave Smith authored the proposal for and co-created **MIDI (Musical Instrument Digital Interface)**, the universal open standard that unified musical hardware worldwide. Later, he pioneered software synthesis with the Seer Systems Reality engine.

`sk3`'s dynamic polyphonic voice pooling, group allocation, and first-class hardware MIDI integration (`src/midi.c` and `src/vendor/minimidio/`) stand directly upon Dave Smith's enduring standards.

---

## 7. Strongly-Timed Live Audio Coding: Ge Wang (ChucK)

- **Pioneer**: [Ge Wang](https://ccrma.stanford.edu/~gow/)
- **Institution**: [Stanford University CCRMA (Center for Computer Research in Music and Acoustics)](https://ccrma.stanford.edu/)
- **Language**: [ChucK: Strongly-Timed Concurrent Audio Programming Language](https://chuck.stanford.edu/)
- **Book**: *The Art of Virtual Acoustic Design*

Ge Wang introduced the concept of **strongly-timed audio programming** through ChucK. In ChucK, time is not an implicit side-effect of audio buffers; time is an explicit, first-class language construct (`100::ms => now`). This paradigm enabled concurrent "shreds" to synchronize sample-accurately and opened new horizons for live coding and on-the-fly computer music performance.

`sk3` and the Skode runtime adopt this deterministic approach to time: scheduled events, micro-delays, and pattern ticks execute deterministically with explicit clock control, bridging live typing with sample-accurate synthesis.

---

## 8. Real-Time Dataflow & Computer Music: Miller Puckette (Max & Pure Data)

- **Pioneer**: [Miller Puckette](https://msp.ucsd.edu/)
- **Institutions**: [IRCAM](https://www.ircam.fr/) | [UC San Diego Department of Music](https://music.ucsd.edu/)
- **Environments**: [Pure Data (Pd)](https://puredata.info/) | [Max (Cycling '74)](https://cycling74.com/)
- **Textbook**: [*The Theory and Technique of Electronic Music*](https://msp.ucsd.edu/techniques.htm)

Miller Puckette designed **Max** at IRCAM in the mid-1980s and subsequently developed **Pure Data (Pd)** in the 1990s as an open-source real-time graphical programming environment for audio, video, and graphical processing. His dual-rate architecture—distinguishing high-rate audio signal vectors (`~`) from asynchronous control message processing—has formed the theoretical backbone of modern digital audio software.

`sk3` honors Miller Puckette's principles through its strict separation of high-priority audio render callbacks from non-blocking lockless control event dispatchers (`src/control-events.h`, `src/skqueue.c`), ensuring audio callback execution remains realtime-safe and glitch-free.
