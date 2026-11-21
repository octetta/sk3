# Binary Opcode Protocol (BOP) Specification & Integration Guide

Version: **1.4.0**  
Transport: **WebSocket (RFC 6455 Frame Opcode `0x2`), UDP Datagrams, C API, WASM / AudioWorklet `SharedArrayBuffer`**

---

## 1. Executive Summary

The **Binary Opcode Protocol (BOP)** is a high-velocity, zero-allocation binary control interface for the `sk3` sound engine. 

While `sk3`'s ASCII Forth interface (`skode`) is optimized for interactive livecoding, Forth scripting, and human readability, real-time control streams (such as 60–120 Hz XY filter sweeps, Amiga MOD tracker replayers, Commodore 64 SID emulators, Urbit Hoon Gall agents, and Web AudioWorklets) require microsecond-latency dispatch with zero tokenizer overhead and zero garbage collection memory pressure.

BOP satisfies these demands with:
1. **Fixed 40-Byte Cache-Line Aligned Struct**: Header (8 bytes) + 8 IEEE 754 32-bit floats (32 bytes). Fits completely within a single 64-byte CPU cache line.
2. **Variable-Length Wire Framing (`8 + 4*N` bytes)**: Slashes network and socket bandwidth for low-arity opcodes (12 bytes for 1 arg, 16 bytes for 2 args).
3. **Multi-Message Batch Dispatch**: Send tens to hundreds of opcode updates in a single datagram or WebSocket binary frame.
4. **Silence on Success**: Zero reverse network traffic on valid opcode execution, preventing reverse bufferbloat and packet storms.

---

## 2. Binary Framing & Memory Layout

### 2.1 Visual Memory & Wire Map

```text
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Opcode (uint16_t)       |        Voice (int16_t)        |  Bytes 0..3
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  argc (uint8) | flags (uint8) |       Reserved (uint16_t)     |  Bytes 4..7
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    args[0] (IEEE 754 Float32)                 |  Bytes 8..11
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    args[1] (IEEE 754 Float32)                 |  Bytes 12..15
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              ...                              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                    args[7] (IEEE 754 Float32)                 |  Bytes 36..39
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Byte Offset | Field | Type | Description |
| :---: | :--- | :--- | :--- |
| `0..1` | `opcode` | `uint16_t` (Little-Endian) | Opcode ID (0..65535). Built-ins (1..127) or discrete opcodes (128+). |
| `2..3` | `voice` | `int16_t` (Little-Endian) | Target voice index (0..63), or `-1` for polyphonic broadcast. |
| `4` | `argc` | `uint8_t` | Number of active float arguments (0..8). |
| `5` | `flags` | `uint8_t` | `0x00` = Variable-length wire frame (`8 + 4*argc` bytes); `0x01` = Fixed 40-byte stride. |
| `6..7` | `reserved` | `uint16_t` | Reserved for alignment / future flags (must be 0). |
| `8..11` | `args[0]` | `float` (IEEE 754 LE) | 1st parameter (e.g. frequency, amplitude, cutoff). |
| `12..15`| `args[1]` | `float` (IEEE 754 LE) | 2nd parameter (e.g. resonance, detune). |
| `...` | `args[2..7]` | `float` (IEEE 754 LE) | Parameters 3 through 8 (e.g. delay parameters, step programs). |

### 2.2 Polyglot Client Definitions

Different clients and languages consume this layout through their native idioms:

#### C / C++ (Packed Struct)
```c
#define SKRED_OPCODE_MSG_MAX_ARGS 8
#define SKRED_BOP_FLAG_FIXED 0x01

#pragma pack(push, 1)
typedef struct {
    uint16_t opcode;    /* Bytes 0..1: Opcode ID (SKODE_OP_* or discrete opcode ID) */
    int16_t  voice;     /* Bytes 2..3: Target voice (0..63), or -1 for broadcast */
    uint8_t  argc;      /* Byte 4:     Number of float arguments (0..8) */
    uint8_t  flags;     /* Byte 5:     Flags (0x01 = fixed 40-byte record stride) */
    uint16_t reserved;  /* Bytes 6..7: Reserved / padding */
    float    args[SKRED_OPCODE_MSG_MAX_ARGS]; /* Bytes 8..39: IEEE 754 32-bit float args */
} skred_opcode_msg_t;
#pragma pack(pop)
```

#### Python / NumPy (AI, SciPy, Jupyter, TouchDesigner)
```python
import struct
import numpy as np

# Format string: < (little-endian), H (uint16), h (int16), B (uint8), B (uint8), x (1 pad), x (1 pad), 8f (8 floats)
BOP_STRUCT_FMT = "<HhbBx8f"
BOP_HEADER_FMT = "<HhbBx"

def pack_bop(opcode: int, voice: int, args: list[float], fixed: bool = False) -> bytes:
    argc = len(args)
    flags = 1 if fixed else 0
    header = struct.pack("<HhBBH", opcode, voice, argc, flags, 0)
    payload = struct.pack(f"<{argc}f", *args) if not fixed else struct.pack("<8f", *args + [0.0]*(8-argc))
    return header + payload

# NumPy Structured Dtype for zero-copy streaming arrays:
bop_dtype = np.dtype([
    ('opcode', '<u2'),
    ('voice', '<i2'),
    ('argc', 'u1'),
    ('flags', 'u1'),
    ('reserved', '<u2'),
    ('args', '<f4', (8,))
])
```

#### Rust (Game Dev, Bevy, Embedded)
```rust
#[repr(C, packed)]
#[derive(Debug, Clone, Copy)]
pub struct SkredOpcodeMsg {
    pub opcode: u16,
    pub voice: i16,
    pub argc: u8,
    pub flags: u8,
    pub reserved: u16,
    pub args: [f32; 8],
}
```

#### Gleam (skode, mod-to-skode, sid-to-skode)
```gleam
import gleam/list

pub fn pack_bop(opcode: Int, voice: Int, args: List(Float)) -> BitArray {
  let argc = list.length(args)
  let args_bits = list.fold(args, <<>>, fn(acc, f) {
    <<acc:bits, f:float-32-little>>
  })
  <<opcode:16-little, voice:16-signed-little, argc:8, 0:8, 0:16, args_bits:bits>>
}
```

#### TypeScript / JavaScript (WebAudio, Strudel, skred-web)
```typescript
export interface BopMessage {
  opcode: number;
  voice: number;
  args: number[];
}

export function packBop(msg: BopMessage): ArrayBuffer {
  const byteLength = 8 + msg.args.length * 4;
  const buffer = new ArrayBuffer(byteLength);
  const view = new DataView(buffer);
  view.setUint16(0, msg.opcode, true);
  view.setInt16(2, msg.voice, true);
  view.setUint8(4, msg.args.length);
  view.setUint8(5, 0); // flags = variable wire length
  view.setUint16(6, 0, true);
  for (let i = 0; i < msg.args.length; i++) {
    view.setFloat32(8 + i * 4, msg.args[i], true);
  }
  return buffer;
}
```

#### BEAM / Elixir
```elixir
def pack_bop(opcode, voice, args) when is_list(args) do
  argc = length(args)
  floats = for f <- args, into: <<>>, do: <<f::float-little-32>>
  <<opcode::little-16, voice::little-signed-16, argc::8, 0::8, 0::16, floats::binary>>
end
```

#### Urbit (Hoon)
```hoon
|=  [op=@ud v=@ud args=(list @rs)]
^-  @ux
%+  can  3
:~  [2 op]
    [2 v]
    [1 (lent args)]
    [1 0]
    [2 0]
    [4 `@ux`(snag 0 args)]
==
```

### 2.3 Wire Framing Modes

- **Packed Variable-Length Framing (Default, `flags & 0x01 == 0`)**:
  - Record Size: `8 + (argc * 4)` bytes.
  - `argc = 0` (e.g. voice reset): **8 bytes**.
  - `argc = 1` (e.g. freq, amp, cutoff, gate): **12 bytes**.
  - `argc = 2` (e.g. filter freq + resonance): **16 bytes**.
  - `argc = 8` (e.g. step program, multi-param delay): **40 bytes**.
- **Fixed-Length Framing (`flags & 0x01 != 0`)**:
  - Record Size: **40 bytes** always. Allows streaming arrays of `skred_opcode_msg_t batch[N]` directly from contiguous memory or ringbuffers.

All integer and float fields are encoded in **standard little-endian** (native to x86_64, ARM, WebAssembly, and modern microcontrollers).

---

## 3. Core C API Dispatchers

| Function | Description |
| :--- | :--- |
| `int skred_dispatch_opcode(int voice, uint16_t opcode, const float *args, int argc);` | Direct dispatch bypassing Forth parser. Realtime-safe. Broadcasts if `voice = -1`. |
| `int skred_dispatch_word(int voice, const char *word, const float *args, int argc);` | Dispatches word by name/alias (`"f"`, `"freq"`, `"a"`, `"cutoff"`). |
| `int skred_dispatch_msg(const skred_opcode_msg_t *msg);` | Dispatches an unpacked 40-byte BOP message struct. |
| `int skred_dispatch_batch(const void *buf, size_t bytes, int *processed);` | Parses and executes a contiguous batch of variable or fixed BOP messages. |

---

## 4. Key Client Implementations

### 4.1 Commodore 64 SID-to-Skode & Amiga MOD-to-Skode Replay

C64 SID emulators and Amiga ProTracker replayers update voice parameters at 50 Hz (PAL) or 60 Hz (NTSC) raster frame ticks.

#### Benefits of BOP for Chiptune Replay:
- **Zero Allocations**: Eliminates `sprintf` and tokenization on every tick.
- **Microsecond Jitter**: Pitch slides, vibrato, and pulse width modulation apply immediately to the synth oscillator voice table.
- **Batching**: All voice updates for a tick pack into a single 48-byte UDP packet or WebSocket frame.

```c
/* Example: SID Raster Tick Emulation Dispatch in C */
void sid_tick_callback(int voice, float freq_hz, float pulse_width, int gate) {
    uint8_t batch[36];
    uint8_t *ptr = batch;

    /* 1. Set Frequency (12 bytes) */
    skred_opcode_msg_t m_freq = { .opcode = SKODE_OP_FREQ, .voice = voice, .argc = 1, .args = {freq_hz} };
    memcpy(ptr, &m_freq, 8); memcpy(ptr + 8, m_freq.args, 4); ptr += 12;

    /* 2. Set Pulse Width / Mod (12 bytes) */
    skred_opcode_msg_t m_pw = { .opcode = SKODE_OP_PHASE_DISTORTION, .voice = voice, .argc = 1, .args = {pulse_width} };
    memcpy(ptr, &m_pw, 8); memcpy(ptr + 8, m_pw.args, 4); ptr += 12;

    /* 3. Gate Trigger / Release (12 bytes) */
    skred_opcode_msg_t m_gate = { .opcode = SKODE_OP_TRIGGER, .voice = voice, .argc = 1, .args = {gate ? 1.0f : 0.0f} };
    memcpy(ptr, &m_gate, 8); memcpy(ptr + 8, m_gate.args, 4); ptr += 12;

    /* Dispatch batch to sk3 */
    int processed = 0;
    skred_dispatch_batch(batch, sizeof(batch), &processed);
}
```

---

### 4.2 Browser & WebAssembly: `skred-web` & AudioWorklet

In browser environments, sending high-rate text strings to WebAssembly triggers JavaScript garbage collection pauses. BOP provides two zero-GC paths:

#### Path A: Over WebSocket (RFC 6455 Binary Frame `0x2`)
```javascript
// Browser WebSocket Binary Dispatch (60–120 Hz XY Pad)
const ws = new WebSocket("ws://localhost:60440/-ws");
ws.binaryType = "arraybuffer";

function sendBopCutoffResonance(voice, cutoffHz, res) {
    const buffer = new ArrayBuffer(16); // 8-byte header + 2 * 4-byte floats
    const view = new DataView(buffer);

    // Header
    view.setUint16(0, 49, true);  // SKODE_OP_FILTER_FREQ
    view.setInt16(2, voice, true); // Voice index
    view.setUint8(4, 2);           // argc = 2
    view.setUint8(5, 0);           // flags = 0 (packed)
    view.setUint16(6, 0, true);    // reserved

    // Arguments
    view.setFloat32(8, cutoffHz, true);
    view.setFloat32(12, res, true);

    ws.send(buffer); // Emits WebSocket Opcode 0x2 (Binary Frame)
}
```

#### Path B: AudioWorklet & `SharedArrayBuffer` Ringbuffer
```javascript
// In AudioWorkletProcessor: Draining SAB ringbuffer directly into WASM
process(inputs, outputs, parameters) {
    let msgBytes;
    while ((msgBytes = ringBuffer.readAvailable()) >= 8) {
        const ptr = Module._malloc(msgBytes);
        ringBuffer.copyTo(Module.HEAPU8.subarray(ptr, ptr + msgBytes));
        
        let processedPtr = Module._malloc(4);
        Module._skred_dispatch_batch(ptr, msgBytes, processedPtr);
        
        Module._free(processedPtr);
        Module._free(ptr);
    }
    Module._skred_process_stereo(outL_ptr, outR_ptr, 128);
    return true;
}
```

---

### 4.3 Urbit (Mars / Hoon Gall Agent)

Urbit Gall agents manipulate nouns and atoms. Converting atoms to ASCII text tapes in Hoon is computationally expensive. With BOP, Hoon packs raw `@ux` bitstrings:

```hoon
:: Example Hoon generator to pack a BOP frequency event
:: opcode: 29 (SKODE_OP_FREQ), voice: 0, argc: 1, arg: 440.0 (0x43dc0000)
|=  [v=@ud freq-hz=@rs]
^-  @ux
%+  can  3
:~  [2 29]               :: opcode: 2-byte little endian
    [2 v]                :: voice:  2-byte little endian
    [1 1]                :: argc:   1-byte (1 float)
    [1 0]                :: flags:  1-byte (packed)
    [2 0]                :: reserved: 2-byte
    [4 `@ux`freq-hz]     :: IEEE 754 32-bit float atom
==
:: Sends 12 bytes directly to %iris HTTP or %eyre WebSocket channel
```

---

### 4.4 BEAM (Elixir / Erlang / Gleam)

Elixir binary pattern matching constructs packed BOP frames natively in a single expression:

```elixir
defmodule Sk3.Bop do
  @op_freq 29
  @op_amp 28
  @op_trigger 60

  def pack_event(opcode, voice, args) when is_list(args) do
    argc = length(args)
    floats = for f <- args, into: <<>>, do: <<f::float-little-32>>
    <<opcode::little-16, voice::little-signed-16, argc::8, 0::8, 0::16, floats::binary>>
  end

  def send_trigger(socket, host, port, voice, freq, amp) do
    # Pack 3 events into one 36-byte UDP datagram
    pkt = pack_event(@op_freq, voice, [freq]) <>
          pack_event(@op_amp, voice, [amp]) <>
          pack_event(@op_trigger, voice, [1.0])

    :gen_udp.send(socket, host, port, pkt)
  end
end
```

---

### 4.5 Livecoding & Pattern Generators (Strudel, TidalCycles, SuperCollider)

#### SuperCollider:
```supercollider
// SuperCollider NetAddr raw binary UDP transmission
n = NetAddr("127.0.0.1", 60440);

~sendBop = { |voice, freq|
    var b = Int8Array.newClear(12);
    // Write Little-Endian Opcode (29) & Voice
    b.put(0, 29); b.put(1, 0);
    b.put(2, voice); b.put(3, 0);
    b.put(4, 1); // argc
    b.put(5, 0); // flags
    b.put(6, 0); b.put(7, 0); // reserved
    // Write Float32 (args[0])
    b.putFloat(8, freq);
    n.sendRaw(b);
};
~sendBop.(0, 440.0);
```

---

## 5. Standard Built-in Realtime Opcode Table

Commonly used opcodes for direct BOP dispatching:

| Opcode ID | Constant | Args (`argc`) | Description |
| :---: | :--- | :---: | :--- |
| `28` | `SKODE_OP_AMP` | 1 | Amplitude (0.0 .. 1.0+) |
| `29` | `SKODE_OP_FREQ` | 1 | Frequency in Hertz (0.01 .. 24000.0) |
| `30` | `SKODE_OP_MIDI_NOTE` | 1..2 | MIDI Note number (0 .. 127) + optional detune |
| `31` | `SKODE_OP_PAN` | 1 | Pan (-1.0 left .. +1.0 right) |
| `37` | `SKODE_OP_PHASE_DISTORTION` | 1 | Phase distortion / pulse width (0.0 .. 1.0) |
| `48` | `SKODE_OP_FILTER_MODE` | 1 | Filter mode (0=off, 1=lp, 2=hp, 3=bp, 4=notch, 5=comb) |
| `49` | `SKODE_OP_FILTER_FREQ` | 1 | Filter cutoff frequency in Hertz |
| `55` | `SKODE_OP_FILTER_RESONANCE`| 1 | Filter resonance (Q factor) |
| `58` | `SKODE_OP_VOICE_RESET` | 0 | Resets voice parameters to defaults |
| `60` | `SKODE_OP_TRIGGER` | 1 | Voice trigger / gate (1.0 = gate on, 0.0 = release) |
| `62` | `SKODE_OP_WAVE` | 1 | Wavetable selection (0=sine, 1=tri, 2=saw, 3=square, etc.) |
| `69` | `SKODE_OP_DELAY_PARAMS` | 1..7 | Delay time, feedback, damping, wet, grit, pan, spread |
| `72` | `SKODE_OP_POLY_NOTE` | 4..5 | Polyphonic note (group, key, vel, note, detune) |
| `73` | `SKODE_OP_POLY_RELEASE` | 2..3 | Polyphonic note release |
| `128+`| Discrete Opcodes | 1..8 | High-level synthesis macros and DSP modifiers |

---

## 6. Code Generation & Contract Architecture (`mkopcodes` & `mksynth`)

All supported opcodes, their discrete opcode IDs (128+), clause arities, parameter matching constraints, and reference documentation are **100% declaratively generated** from modular `.def` schema specifications:

```
 schema/opcodes/*.def  ──> tools/mkopcodes ──> src/gen/skode_dict_gen.h (Enums & Clauses)
                                           ──> src/gen/skred_opcodes_gen.c (RT Dispatch Table)
                                           ──> doc/REFERENCE.md (Full Opcode Manual)
                                           ──> src/gen/skode-doc.inc (In-Engine REPL Help)

 schema/synth/*.def    ──> tools/mksynth   ──> src/gen/synth_state_gen.h (Voice States)
                                           ──> src/gen/synth_voice_ops_gen.c (Voice Ops)
                                           ──> src/gen/test_schema_bounds_gen.c (Tests)
                                           ──> ../skode/src/skode/gen/*.gleam (Gleam Bindings)
```

1. **`tools/mkopcodes`**:
   - Parses `schema/opcodes/*.def` covering 304 words and 613 clauses.
   - Automatically assigns discrete realtime opcode IDs starting at `SKODE_OP_DISCRETE_BASE = 128` (up to `SKODE_REALTIME_OPS_CAPACITY = 1024`).
   - Generates [doc/REFERENCE.md](REFERENCE.md), which documents every word, clause arity, execution model (Realtime vs Immediate), and discrete opcode ID.
   - Runs on `make gen` in the repository root.

2. **`tools/mksynth`**:
   - Compiles `schema/synth/*.def` specifying all voice parameters (frequency, amplitude, envelope stages, wave tables, filters, links).
   - Guarantees strict boundary invariants and compiles directly to both C headers and typed Gleam source files in `skode/src/skode/gen/`.

---

## 7. Gleam / Skode Ecosystem & Chiptune Integration

The Gleam client suite (`skode`, `mod-to-skode`, `sid-to-skode`, and `skode/app/editor`) integrates with BOP through a unified architecture:

### 7.1 Gleam `BitArray` Driver
Gleam programs can pack BOP frames directly using type-safe bitstrings without FFI glue:

```gleam
import gleam/list

pub fn pack_opcode(opcode: Int, voice: Int, args: List(Float)) -> BitArray {
  let argc = list.length(args)
  let payload = list.fold(args, <<>>, fn(acc, val) {
    <<acc:bits, val:float-32-little>>
  })
  <<opcode:16-little, voice:16-signed-little, argc:8, 0:8, 0:16, payload:bits>>
}
```

### 7.2 `sid-to-skode` & `mod-to-skode` Acceleration
In the chiptune replayers, the audio driver previously serialized register changes into ASCII Forth commands (`v0 f440 w2 a0.8`) on every 50/60 Hz raster tick. 

Under BOP:
- Each voice register update maps directly to a 12-byte packed message (`SKODE_OP_FREQ`, `SKODE_OP_AMP`, `SKODE_OP_PHASE_DISTORTION`, `SKODE_OP_TRIGGER`).
- All 3 SID voices or 4 MOD channels for a tick are batched into a contiguous 36–48 byte typed `Uint8Array`.
- In WebAssembly, this is dispatched directly via `_skred_dispatch_batch(ptr, len, processed)` or over a binary WebSocket frame, completely eliminating JavaScript string allocation and garbage collection pauses.

### 7.3 `skode` Editor App
The `skode` editor app utilizes a dual-path architecture:
- **ASCII Forth Path**: Used for the text editor, script saving, macro definitions, and REPL interactions where human readability and Forth dynamism are desired.
- **BOP Binary Path**: Used for high-rate continuous parameter controls (XY controller pads, virtual sliders, modulation wheels, and live oscilloscope data streaming) with sub-millisecond, zero-allocation response.
