# sk3 Polymorphic TCP, HTTP & WebSocket Server Reference

`sk3` features a built-in, zero-dependency, multiplexed polymorphic TCP server. Operating on a single port (**`60440`** by default), the server automatically senses incoming traffic and escalates each client connection to the appropriate protocol:

1. **Raw TCP / ASCII Line Stream**: Direct, zero-overhead Forth REPL for Netcat (`nc`), `socat`, Tcl `socket`, embedded microcontrollers, and hardware controllers.
2. **HTTP/1.1 REST**: Standard POST/GET requests for cURL, Python, Node.js, and browser `fetch()` with full CORS and Chromium Private Network Access (PNA).
3. **RFC 6455 WebSocket**: Full-duplex framing for web applications, interactive visualizers, and continuous XY controller pads streaming parameter sweeps at 60–120 Hz.

---

## 1. Core Architecture & Protocol Escalation Ladder

Every incoming connection starts in an auto-detect state (`SK_PROTO_DETECT`) and locks into its protocol based on its first payload:

```
                      [ TCP Client Connects ]
                                 │
                     First packet arrives (Probe)
                                 │
         ┌───────────────────────┴────────────────────────┐
         │                                                │
Starts with HTTP Method                          Starts with Skode / ASCII
(`GET `, `POST `, `OPTIONS `)                    (`v0`, `n60`, `[macro]`, `-status`)
         │                                                │
         ▼                                                ▼
  [ HTTP/1.1 Mode ]                             [ Raw TCP / ASCII Mode ]
  • REST / cURL                                 • Netcat (`nc localhost 60440`)
  • Web browser `fetch()`                       • `socat`, Tcl `socket`, Telnet
  • CORS & Private Network Access               • Zero protocol framing overhead
         │                                      • Immediate interactive REPL
         ▼
  Headers include:
  `Upgrade: websocket`
         │
         ▼
  [ RFC 6455 WebSocket ]
  • Browser XY Pad streaming @ 60–120 Hz
  • Persistent Forth macros & state
  • Full-duplex framing, ping/pong
```

### Server Semantics
* **Default Port**: **`60440`** (Identical port number to UDP control; TCP and UDP operate in independent OS protocol spaces without collision).
* **CLI Flags**:
  - `-T<port>` or `--tcp <port>`: Set listening TCP port (e.g. `-T60440`).
  - `-H<port>` or `--http <port>`: Alias for `--tcp`.
  - Pass `-T0` to disable the TCP server entirely.
* **Low-Latency Multiplexing**:
  - `TCP_NODELAY` is enabled on every accepted client socket, disabling Nagle's algorithm for microsecond packet dispatch.
  - A single dedicated worker thread multiplexes up to 64 concurrent client connections using non-blocking `select()`.
* **Per-Connection Isolated Context (`skode_t`)**:
  - Every connection owns an independent `skode_t` Forth execution context.
  - Macros (`[name]:...;`), registers (`=0,val`), voice parameters, and execution state defined in one connection remain completely isolated from other connections.
  - When the client disconnects, `skode_free` is automatically invoked, releasing all memory deterministically with zero leaks.
* **Continuous Connection Support (`Keep-Alive`)**:
  - Full HTTP/1.1 `Connection: keep-alive` support with a 60-second idle timeout.
* **CORS & Chromium Private Network Access (PNA)**:
  - Built-in `Access-Control-Allow-Origin: *` and `Access-Control-Allow-Private-Network: true` headers on all responses, with automatic handling of `OPTIONS` preflight requests (204 No Content).
* **WASM Safety**:
  - Completely stubbed out at compile time in WebAssembly builds (`__EMSCRIPTEN__` and `SK_NO_NETWORK`).

---

## 2. In-Engine Diagnostics: The `/s` Command

Within the `sk3` REPL, executing `/s` displays active rendering voices, UDP listener status, and the polymorphic TCP server status, including an itemized list of all active clients and their connection modes:

```forth
/s
```

Output:
```
# active_voices_rendered 0
# udp_port 60440 (OK) udp_events_port 0 (Off/Fail)
# tcp_port 60440 (OK) clients 3 (raw:1 http:1 ws:1)
#   client [0] 127.0.0.1:46780 raw
#   client [1] 127.0.0.1:46786 http
#   client [2] 127.0.0.1:46802 ws
```

---

## 3. Endpoints & Command Conventions

To avoid collision with Skode's native `/` operator prefix (such as `/ks` for loading wavetables or `/d` for data tables), administrative commands use the **`-command`** (or **`.command`**) convention.

| Protocol | Method / Path | Payload Format | Description |
| :--- | :--- | :--- | :--- |
| **WebSocket** | `ws://localhost:60440/` | RFC 6455 Text Frame | Full-duplex live streaming, macros, and parameter updates. Replies with text frame (`OK\n` or query data). |
| **Raw TCP** | `localhost:60440` | ASCII lines (`\n`) | Direct line-by-line Skode command streaming. Replies with raw ASCII (`OK\n` or query data). |
| **HTTP** | `POST /` | `text/plain` | Evaluates Skode command body in the connection's dedicated context. |
| **HTTP** | `POST /-skode` or `/.skode` | `text/plain` | Explicit endpoint for Skode evaluation (synonymous with `/`). |
| **HTTP** | `POST /-reset` or `/.reset` | `text/plain` | Resets the connection's `skode_t` context, clearing all macros. |
| **HTTP** | `GET /` | `text/plain` | Welcome banner with endpoint overview and version. |
| **HTTP** | `GET /-status` or `/.status` | `application/json` | JSON health check and metrics (`active_connections`, `raw_clients`, `http_clients`, `ws_clients`, `total_requests`, `total_commands`, `errors`). |
| **HTTP** | `GET /-version` or `/.version`| `text/plain` | Returns current engine version string (e.g. `1.3.20\n`). |
| **HTTP** | `OPTIONS *` | *(none)* | CORS preflight: returns HTTP 204 No Content with PNA headers. |

---

## 4. Skode Language Syntax Cheat Sheet

Skode commands are compact. Commands consist of an operator letter or punctuation followed immediately by numeric arguments:

* **Voice Selection**: `v0` (selects voice 0; valid 0–255).
* **Waveform**: `w0` (selects wave slot 0; `w0,1,0` specifies interpolation & loop).
* **Pitch**:
  - `f440` (sets frequency to 440 Hz).
  - `n60` (sets pitch to MIDI note 60 = Middle C).
  - `n69` (sets pitch to MIDI note 69 = A4 440 Hz).
* **Amplitude**: `a0` (0 dB, full volume), `a-6` (-6 dB attenuation).
* **Gate / Velocity**: `l1` (gate on with velocity 1.0), `l0` (gate off / release).
* **Filter Mode**: `J0` (lowpass), `J1` (highpass), `J2` (bandpass).
* **Filter Cutoff**: `K1200` (cutoff at 1200 Hz), `Q2.0` (resonance Q).
* **Envelopes**: `t10,80,0.5,300` (Attack 10ms, Decay 80ms, Sustain 0.5, Release 300ms).
* **State Query**: `v0 ?` (inspect voice 0), `??` (inspect all voices).
* **Macro Definition**: `[lead]:v0 w0 f440 a-3 l1; lead` (defines macro `lead` and invokes it).
* **Registers**: `=0,440 v0 f$0 l1` (assign register 0 to 440, set voice frequency from `$0`, trigger gate).
* **Polyphonic Pool**: `pn0,1,60,1` (pool 0, key 1, note 60, velocity 1), `pr0,1` (release).

---

## 5. WebSocket Control: Browser XY Controller Pad Example

WebSockets provide the lowest possible latency for browser web apps streaming high-frequency continuous parameter updates (e.g. from mouse/touch XY pads, MIDI controllers, or automated envelopes) directly into local or remote `sk3` instances.

### Multi-Instance Architecture
A single web application can control multiple `sk3` instances running on different ports:

```
┌─────────────────────────────────────────────────────────────┐
│                       Browser Web App                       │
│                                                             │
│   ws://localhost:60440        ws://localhost:60441          │
│   (Instance 1: Bass / Beat)   (Instance 2: Poly Synth)      │
└──────────────┬──────────────────────────────┬───────────────┘
               │                              │
               ▼                              ▼
     ┌──────────────────┐           ┌──────────────────┐
     │  sk3 Port 60440  │           │  sk3 Port 60441  │
     └──────────────────┘           └──────────────────┘
```

### Complete HTML / JavaScript XY Controller Pad

Save this snippet as `xy_pad.html` and open it directly in your browser:

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>sk3 WebSocket XY Filter Pad</title>
  <style>
    body { font-family: sans-serif; background: #121214; color: #e1e1e6; text-align: center; margin: 20px; }
    #pad { width: 400px; height: 400px; margin: 20px auto; background: #1f1f23; border: 2px solid #50fa7b; border-radius: 8px; position: relative; cursor: crosshair; touch-action: none; }
    #cursor { width: 14px; height: 14px; background: #ff5555; border-radius: 50%; position: absolute; transform: translate(-50%, -50%); pointer-events: none; }
    .status { font-family: monospace; color: #8be9fd; }
  </style>
</head>
<body>
  <h2>sk3 Interactive WebSocket XY Pad</h2>
  <div class="status" id="status">Connecting to ws://localhost:60440...</div>
  <div id="pad"><div id="cursor" style="left: 200px; top: 200px;"></div></div>
  <p>X: Filter Cutoff (100 Hz – 8000 Hz) | Y: Resonance Q (0.5 – 10.0)</p>

  <script>
    const statusEl = document.getElementById('status');
    const pad = document.getElementById('pad');
    const cursor = document.getElementById('cursor');

    // 1. Establish persistent WebSocket to local sk3 instance
    const ws = new WebSocket('ws://localhost:60440/-ws');

    ws.onopen = () => {
      statusEl.textContent = 'Connected to sk3 (Port 60440)';
      // Setup Voice 0: Lowpass filter (J0), Saw wave (w0), 110 Hz base tone, gate on
      ws.send('v0 w0 f110 J0 K1200 Q2.0 a0 l1\n');
    };

    ws.onclose = () => {
      statusEl.textContent = 'Disconnected from sk3';
    };

    ws.onmessage = (event) => {
      // Receives 'OK\n' or query diagnostics from engine
    };

    // 2. Stream XY parameter updates continuously on pointermove (60–120 Hz)
    function handlePointer(e) {
      if (ws.readyState !== WebSocket.OPEN) return;
      const rect = pad.getBoundingClientRect();
      const x = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
      const y = Math.max(0, Math.min(1, 1 - (e.clientY - rect.top) / rect.height));

      cursor.style.left = (x * 400) + 'px';
      cursor.style.top = ((1 - y) * 400) + 'px';

      // Map X to logarithmic filter cutoff (100 Hz - 8000 Hz)
      const cutoff = Math.round(100 * Math.pow(80, x));
      // Map Y to linear resonance Q (0.5 - 10.0)
      const resonance = (0.5 + y * 9.5).toFixed(2);

      // Stream high-frequency update frame
      ws.send(`v0 K${cutoff} Q${resonance}\n`);
    }

    pad.addEventListener('pointerdown', (e) => {
      pad.setPointerCapture(e.pointerId);
      handlePointer(e);
      pad.addEventListener('pointermove', handlePointer);
    });

    pad.addEventListener('pointerup', () => {
      pad.removeEventListener('pointermove', handlePointer);
    });
  </script>
</body>
</html>
```

---

## 6. Raw TCP Mode: Direct ASCII Streaming (Netcat, socat, Tcl)

When you connect directly via TCP without sending HTTP headers, `sk3` operates as a raw ASCII streaming server.

### A. Netcat (`nc`)

#### 1. One-Shot Command
```bash
echo "v0 w0 f440 a0 l1" | nc -N localhost 60440
```

#### 2. Interactive Terminal REPL
```bash
nc localhost 60440
```
Type any command interactively; responses return instantly:
```forth
v0 w0 f440 a0 l1
OK
[lead]:v0 n60 l1; lead
OK
v0 ?
OK
-status
{"status":"ok","version":"1.3.20","port":60440,"active_connections":1,"raw_clients":1,"http_clients":0,"ws_clients":0,"total_requests":4,"total_commands":4,"errors":0}
```

### B. `socat`
```bash
echo "v0 w0 f440 a0 l1" | socat - TCP:localhost:60440
```

### C. Tcl Socket
```tcl
# Connect raw TCP socket to sk3
set s [socket localhost 60440]
fconfigure $s -buffering line

# Send Skode command
puts $s "v0 w0 f440 a0 l1"
puts [gets $s]  ;# Returns "OK"

# Query voice status
puts $s "v0 ?"
puts [gets $s]

close $s
```

---

## 7. HTTP/1.1 REST Mode: cURL, Python & Browser `fetch()`

### A. cURL One-Shot Requests

```bash
# Trigger voice 0: waveform 0, 440 Hz, 0 dB amplitude, velocity 1.0
curl -X POST http://localhost:60440/ -d "v0 w0 f440 a0 l1"

# Trigger voice 0 with a MIDI note (Middle C = n60)
curl -X POST http://localhost:60440/-skode -d "v0 n60 a-3 l1"

# Query current voice state
curl -X POST http://localhost:60440/ -d "v0 ?"

# Query JSON status and metrics
curl -s http://localhost:60440/-status
```

### B. Python `urllib` (Zero Dependencies)

```python
import urllib.request

def send_skode(cmd: str):
    req = urllib.request.Request(
        "http://localhost:60440/-skode",
        data=cmd.encode("utf-8"),
        headers={"Content-Type": "text/plain"}
    )
    with urllib.request.urlopen(req) as resp:
        return resp.read().decode("utf-8")

# Configure voice 0
send_skode("v0 w0 f440 a0 l1")
```

### C. Web Browser `fetch()` (JavaScript)

```javascript
// Fire-and-forget command from any browser webpage or dev console
fetch("http://localhost:60440/-skode", {
  method: "POST",
  headers: { "Content-Type": "text/plain" },
  body: "v0 w0 f440 a0 l1\n"
})
.then(res => res.text())
.then(output => console.log("sk3 output:", output));
```

---

## 8. Summary of Protocol Advantages

| Protocol | Best For | Handshake Overhead | Per-Message Overhead | Latency |
| :--- | :--- | :--- | :--- | :--- |
| **WebSocket** | XY pads, continuous parameter sweeps, browser GUIs | 1 HTTP GET upgrade | 2–6 bytes frame header | < 0.1 ms |
| **Raw TCP** | Terminal REPL, Netcat scripts, microcontrollers, Tcl | None (Instant) | 0 bytes (Raw text + `\n`) | < 0.1 ms |
| **HTTP/1.1** | One-shot cURL, monitoring scrapers, REST APIs | TCP 3-way handshake | ~150 bytes HTTP headers | ~0.5 ms |
