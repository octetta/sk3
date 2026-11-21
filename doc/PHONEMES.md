# ARPAbet Phonetic Speech Synthesis & DSL Reference

`sk3` features an internal, real-time, zero-allocation physical speech synthesizer inspired by historic formant engines (**DECtalk**, **SAM**, and early **Klatt80** models). Speech phrases are written in a bracketed phonetic DSL string using standard **ARPAbet phonemes** and rendered into the `data` array and wavetable slots via `tts!` and `d>w`.

---

## 1. Quick Start

### Two-Step Pipeline (TTS into `data`, then `data` into Wavetable)
```text
[HH:70 EH1:110 L:70 OW1:150] tts!;   # Synthesize phrase into 'data' array
300 d>w;                             # Install data array to wavetable slot 300 (one-shot mode)
300 W;                               # Select wave 300 on active voice
```

### Direct Render & Install
```text
[S:70 P:50 IY1:120 K:60] 300 tts!;   # Render and install directly to slot 300
300 W;
```

### Custom Voice Persona & Base Pitch
```text
# Voice personas: 0=Paul, 1=Harry, 2=Betty, 3=SAM, 4=Robot, 5=Elf
[HH:70 EH1:110 L:70 OW1:150] 301 4 tts!;         # Robot voice into slot 301
[S:60 AY1:140 N:70 TH:80] 302 2 180.0 tts!;      # Betty female voice at 180 Hz into slot 302
```

---

## 2. Phonetic Token DSL Syntax

Phrases are enclosed in square brackets `[...]` containing space-delimited phoneme tokens:

$$\text{[CODE[stress][:duration_ms][@pitch_hz[>glide_hz]][\sim vibrato_rate][*amp]]}$$

| Component | Syntax | Example | Description |
|---|:---:|:---:|---|
| **Phoneme Code** | `[A-Z]{1,2}` | `EH`, `S`, `K` | 40 standard ARPAbet phonetic acoustic targets |
| **Numeric Stress** | `0`, `1`, `2` | `EH1`, `AA0`, `UW2` | `1`: Primary stress (+25% duration, +15% pitch, +20% gain)<br>`0`: Unstressed (-25% duration, -5% pitch, -20% gain)<br>`2`: Secondary stress (+10% duration) |
| **Duration** | `:ms` | `:120`, `:75` | Explicit duration in milliseconds (default: 60–160 ms per phoneme) |
| **Pitch** | `@hz` | `@130`, `@220` | Fundamental frequency $F_0$ in Hz (default: 120 Hz) |
| **Pitch Inflection Glide** | `@start>end` | `@145>105` | Linear pitch glide across token (cadence drop, question rise, tones) |
| **Vibrato** | `~` or `~rate` | `~`, `~6.0` | 2.5% vocal pitch wobble at specified frequency (default: 5.5 Hz) |
| **Amplitude** | `*amp` | `*1.4`, `*0.6` | Relative gain multiplier |
| **Pause / Silence** | `_` | `_:60` | Inter-word silence / rest in milliseconds |

---

## 3. ARPAbet Phoneme Catalog

### Vowels & Diphthongs
| ARPAbet | IPA | Example Word | Default ms | Formant Targets ($F_1, F_2, F_3$ Hz) |
|:---:|:---:|:---|:---:|:---|
| `AA` | ɑ | f**a**ther, h**o**t | 140 | 730, 1090, 2440 |
| `AE` | æ | c**a**t, b**a**t | 140 | 660, 1720, 2410 |
| `AH` | ʌ | b**u**t, c**u**p | 120 | 640, 1190, 2390 |
| `AO` | ɔ | c**augh**t, d**o**g | 140 | 570, 840, 2410 |
| `EH` | ɛ | b**e**d, r**e**d | 120 | 530, 1840, 2480 |
| `ER` | ɝ | b**ir**d, h**er** | 150 | 490, 1350, 1690 |
| `EY` | eɪ | s**ay**, b**ai**t | 150 | 480, 2000, 2600 |
| `IH` | ɪ | b**i**g, s**i**t | 110 | 390, 1990, 2550 |
| `IY` | i | s**ee**, f**ee**t | 130 | 270, 2290, 3010 |
| `OW` | oʊ | g**o**, b**oa**t | 150 | 500, 850, 2250 |
| `UH` | ʊ | b**oo**k, p**u**t | 110 | 440, 1020, 2240 |
| `UW` | u | t**wo**, t**oo** | 130 | 300, 870, 2240 |
| `AY` | aɪ | m**y**, b**i**te | 160 | 650, 1700, 2500 |
| `AW` | aʊ | n**ow**, h**ow** | 160 | 680, 1150, 2400 |
| `OY` | ɔɪ | b**oy**, n**oi**se | 160 | 520, 1600, 2400 |

### Consonants
| ARPAbet | IPA | Category | Example Word | Acoustic Description |
|:---:|:---:|:---|:---|:---|
| `S` | s | Fricative | **s**ee, **s**it | High bandpass noise (6500 Hz, 2400 Hz BW) |
| `SH` | ʃ | Fricative | **sh**ip, wi**sh** | Mid bandpass noise (3600 Hz, 1800 Hz BW) |
| `Z` | z | Voiced Fricative | **z**oo, ro**s**e | Voiced glottal pulse + 6500 Hz noise |
| `ZH` | ʒ | Voiced Fricative | mea**s**ure | Voiced glottal pulse + 3600 Hz noise |
| `F` | f | Fricative | **f**or, lau**gh** | Diffuse high bandpass noise (4500 Hz) |
| `V` | v | Voiced Fricative | **v**ote, ha**ve** | Voiced glottal pulse + 4500 Hz noise |
| `TH` | θ | Fricative | **th**in, ba**th** | High bandpass noise (5000 Hz) |
| `DH` | ð | Voiced Fricative | **th**is, fea**th**er | Voiced glottal pulse + 4000 Hz noise |
| `HH` | h | Aspirate | **h**at, **h**e | Direct vocal tract formant aspiration |
| `CH` | tʃ | Affricate | **ch**ip, mat**ch** | Closure + 3800 Hz release burst |
| `JH` | dʒ | Voiced Affricate | **j**ar, e**dg**e | Voiced closure + 3800 Hz burst |
| `P` | p | Unvoiced Stop | **p**at, to**p** | Labial silent closure + low release pop |
| `B` | b | Voiced Stop | **b**at, tu**b** | Voiced closure + low release pop |
| `T` | t | Unvoiced Stop | **t**op, ca**t** | Alveolar closure + crisp high pop (5200 Hz) |
| `D` | d | Voiced Stop | **d**og, be**d** | Voiced closure + crisp pop (4500 Hz) |
| `K` | k | Unvoiced Stop | **k**ey, boo**k** | Velar closure + resonant burst (2800 Hz) |
| `G` | ɡ | Voiced Stop | **g**o, bi**g** | Voiced closure + velar burst (2500 Hz) |
| `M` | m | Nasal | **m**an, su**m** | Bilabial nasal resonance (250 Hz $F_1$) |
| `N` | n | Nasal | **n**o, su**n** | Alveolar nasal resonance |
| `NG` | ŋ | Nasal | si**ng**, ri**ng** | Velar nasal resonance |
| `L` | l | Liquid | **l**eg, ba**ll** | Lateral resonant glide |
| `R` | ɹ | Liquid | **r**ed, ca**r** | Retroflex resonant glide (low $F_3$) |
| `W` | w | Glide | **w**et, a**w**ay | Labio-velar glide (low $F_1, F_2$) |
| `Y` | j | Glide | **y**es, **y**ou | Palatal glide (high $F_2$) |
| `_` | | Silence | *(pause)* | Zero amplitude inter-word pause |

---

## 4. Voice Personas

Pass the voice ID (`0..5`) to `tts!`:
```text
[tokens...] <slot> <voice_id> tts!;
```

| ID | Name | Inspiration | Acoustic Characteristics |
|:---:|:---|:---|:---|
| `0` | **Paul** | DECtalk *Perfect Paul* | Balanced adult male, standard vocal tract length |
| `1` | **Harry** | DECtalk *Huge Harry* | Deep resonant male, lengthened throat, rich low formants |
| `2` | **Betty** | DECtalk *Beautiful Betty* | Female voice, shortened tract, higher $F_0$ and formant scale |
| `3` | **SAM** | C64 *Software Automatic Mouth* (1982) | Classic 8-bit retro robotic buzz, stepped formant transitions |
| `4` | **Robot** | Kraftwerk (*The Robots* / *Computer World*) | Monotone pitch, zero sentence declination, vocoder buzz |
| `5` | **Elf** | *Kit the Kid* / Child | High pitch, fast cadence, compact child vocal tract |

---

## 5. Speaking Other Languages

Because the engine models physical acoustic resonances rather than English-only diphone recordings, it synthesizes many non-English languages with high clarity.

### 5-Vowel Languages (Spanish, Italian, Japanese, Swahili)
Map foreign vowels directly to the cardinal ARPAbet vowel targets:
- **[a]** $\rightarrow$ `AA`
- **[e]** $\rightarrow$ `EH`
- **[i]** $\rightarrow$ `IY`
- **[o]** $\rightarrow$ `OW` or `AO`
- **[u]** $\rightarrow$ `UW`

#### Examples:
- **Japanese** (*Konnichiwa* / こんにちは):
  ```text
  [K:60 OW:70 N:70 IY:70 CH:70 IY:70 W:60 AA:120] 300 tts!;
  ```
- **Spanish** (*Hola amigo*):
  ```text
  [OW:80 L:70 AA:90 _:40 AA:70 M:70 IY:80 G:70 OW:120] 300 tts!;
  ```
- **Italian** (*Ciao bella*):
  ```text
  [CH:70 AA:90 OW:70 _:40 B:60 EH1:110 L:70 AA:100] 300 tts!;
  ```

### German & French
- **German** (*Guten Tag*):
  ```text
  [G:70 UW:110 T:60 EH0:70 N:70 _:40 T:70 AA1:130 K:80] 300 tts!;
  ```
  - *ä* maps to `EH` or `AE`.
  - *ö* maps to `ER` (or `EH` followed by `UW`).
  - *ü* maps to `IY` blended into `UW`.
- **French** (*Bonjour*):
  ```text
  [B:60 OW:80 N:60 ZH:80 UW:120 R:70] 300 tts!;
  ```
  - Nasal vowels (*an, on, in*) are formed by coupling the vowel with `N` or `NG`.

### Tonal Languages (Mandarin, Cantonese, Vietnamese)
Use the pitch inflection syntax `@start>end` to express pitch tones:
- **Tone 1** (High flat): `[M:60 AA:120@160]`
- **Tone 2** (Rising): `[M:60 AA:120@120>160]`
- **Tone 3** (Dipping): `[M:60 AA:80@130>100 AA:90@100>140]`
- **Tone 4** (Falling): `[M:60 AA:100@165>110]`
- **Mandarin** (*Nǐ hǎo* / 你好):
  ```text
  [N:60 IY:90@120>100 IY:80@100>140 _:30 HH:60 AA:130@145>105] 300 tts!;
  ```

---

## 6. Public Text-to-ARPAbet Resources

To convert English sentences into ARPAbet phoneme sequences:

1. **CMU Pronouncing Dictionary (`cmudict`)**:
   - The authoritative open-source pronunciation database (134,000+ words) created by Carnegie Mellon University.
   - Lookup tool: [http://www.speech.cs.cmu.edu/cgi-bin/cmudict](http://www.speech.cs.cmu.edu/cgi-bin/cmudict)
   - Plain text dictionary format:
     ```text
     SYNTHESIZER  S IH1 N TH AH0 S AY2 Z ER0
     COMPUTER     K AH0 M P Y UW1 T ER0
     ANTIGRAVITY  AE2 N T IY0 G R AE1 V AH0 T IY0
     ```
2. **Python `pronouncing` Library**:
   ```python
   import pronouncing
   phones = pronouncing.phones_for_word("synthesizer")[0]
   print(f"[{phones}] 300 tts!")
   # Output: [S IH1 N TH AH0 S AY2 Z ER0] 300 tts!
   ```
3. **`g2p-en` (Neural Grapheme-to-Phoneme)**:
   For arbitrary sentences, slang, or novel words:
   ```bash
   pip install g2p-en
   python -c "from g2p_en import G2p; print(' '.join(G2p()('Hello world')))"
   # Output: HH AH0 L OW1   W ER1 L D
   ```
