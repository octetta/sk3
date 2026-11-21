# sk3 HTTP Control API Reference

`sk3` features a built-in, zero-dependency, multiplexed HTTP control server. It enables web browsers, command-line scripts, language runtimes, and external DAWs to evaluate Skode commands over standard HTTP POST requests without needing raw UDP, binary OSC, or WebSocket daemons.

---

## 1. Core Architecture & Server Semantics

* **Default Port**: **`60440`** (Identical port number to UDP control; TCP and UDP operate in independent protocol spaces without collision).
* **CLI Flags**:
  - `-H<port>` or `--http <port>`: Set listening TCP port (e.g. `-H8080`).
  - Pass `-H0` to disable HTTP control.
* **Multiplexed I/O (Non-Blocking `select()`)**:
  - A single dedicated worker thread multiplexes up to 64 concurrent client connections.
  - No thread-per-connection overhead; idle connections consume near-zero CPU and memory.
* **Per-Connection Isolated Context (`skode_t`)**:
  - When a TCP connection is established, an independent `skode_t` context is allocated (`skode_init`).
  - Macros (`[name]:...;`), registers (`=0,val`), voice parameters, and execution state defined in one connection remain completely isolated from other connections.
  - When the client disconnects, `skode_free` is automatically invoked, releasing all memory deterministically with zero leaks.
* **Continuous Connection Support (`Keep-Alive`)**:
  - Full HTTP/1.1 `Connection: keep-alive` support with a 60-second idle timeout.
  - Clients can keep the socket open and stream back-to-back command payloads; all state and macros defined earlier in the session remain alive until disconnection.
* **CORS Support**:
  - Built-in `Access-Control-Allow-Origin: *` headers on all responses, with automatic handling of `OPTIONS` preflight requests (204 No Content), allowing web apps and notebooks to control `sk3` directly from any domain.
* **WASM Safety**:
  - Completely stubbed out at compile time in WebAssembly builds (`__EMSCRIPTEN__` and `SK_NO_NETWORK`).

---

## 2. Endpoints & Command Conventions

To avoid collision with Skode's native `/` operator prefix (such as `/ks` for loading wavetables or `/d` for data tables), HTTP administrative commands use the **`-command`** (or **`.command`**) convention.

| Method | Path | Format | Description |
| :--- | :--- | :--- | :--- |
| `POST` | `/` | `text/plain` | Evaluates Skode command body in the connection's dedicated context. Returns captured output log or `OK\n`. |
| `POST` | `/-skode` or `/.skode` | `text/plain` | Explicit endpoint for Skode evaluation (synonymous with `/`). |
| `POST` | `/-cmd` or `/.cmd` | `text/plain` | Convenient alias for command execution. |
| `POST` | `/-reset` or `/.reset` | `text/plain` | Resets the connection's `skode_t` context, clearing all macros and restoring default state. |
| `GET` | `/` | `text/plain` | Welcome banner with endpoint overview and version. |
| `GET` | `/-status` or `/.status` | `application/json` | Live JSON health check and metrics (`active_connections`, `total_requests`, `total_commands`, `errors`). |
| `GET` | `/-metrics` or `/.metrics`| `application/json` | Synonymous with `/-status`. |
| `GET` | `/-version` or `/.version`| `text/plain` | Returns current engine version string (e.g. `1.3.20\n`). |
| `OPTIONS`| `*` | *(none)* | CORS preflight: returns HTTP 204 No Content. |

---

## 3. Skode Language Syntax Cheat Sheet

Skode commands are compact. Commands consist of a letter or punctuation character followed immediately by numeric arguments (commas or spaces separate multiple arguments):

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

## 4. One-Shot Examples (Fire-and-Forget)

One-shot requests establish a TCP connection, execute the Skode commands, return the result, and terminate.

### cURL (Bash / Terminal)

> [!TIP]
> When passing Skode register expressions containing `$` (such as `f$0`), enclose the command in single quotes `'...'` so the Unix shell does not treat `$0` as a shell variable.

```bash
# 1. Trigger voice 0: waveform 0, 440 Hz, 0 dB amplitude, velocity 1.0
curl -X POST http://localhost:60440/ -d "v0 w0 f440 a0 l1"

# 2. Trigger voice 0 with a MIDI note (Middle C = n60)
curl -X POST http://localhost:60440/-skode -d "v0 n60 a-3 l1"

# 3. Configure a resonant lowpass filter: mode 0 (lowpass), cutoff 1200 Hz, Q 2.5
curl -X POST http://localhost:60440/-cmd -d "v0 J0 K1200 Q2.5"

# 4. Query current voice state
curl -X POST http://localhost:60440/ -d "v0 ?"

# 5. Query all active engine voice parameters
curl -X POST http://localhost:60440/ -d "??"

# 6. Stream a multi-line Skode score file from disk
curl -X POST http://localhost:60440/ --data-binary @composition.sk

# 7. Check server status and metrics JSON
curl -s http://localhost:60440/-status

# 8. Check engine version
curl -s http://localhost:60440/-version
```

### Python (`requests`)

```python
import requests

url = "http://localhost:60440/"

# Trigger a sine/saw lead note on voice 0 (A4 440 Hz)
resp = requests.post(url, data="v0 w0 f440 a-3 l1")
print(resp.text)  # Returns "OK\n"

# Query voice 0 status
resp = requests.post(url, data="v0 ?")
print("Voice 0 status:", resp.text)  # e.g., "v0 w0 f440 a-3"

# Query HTTP server metrics
metrics = requests.get("http://localhost:60440/-status").json()
print(f"Connected to sk3 v{metrics['version']} (active connections: {metrics['active_connections']})")
```

### Node.js (`fetch`)

```javascript
// Trigger a chord across voices 0, 1, and 2
const res = await fetch("http://localhost:60440/-skode", {
  method: "POST",
  headers: { "Content-Type": "text/plain" },
  body: "v0 n60 a-6 l1 v1 n64 a-6 l1 v2 n67 a-6 l1"
});
console.log(await res.text());
```

### Browser JavaScript (Web Console or Live GUI)

Because `sk3` emits permissive CORS headers, you can send Skode commands directly from the browser console on any page:

```javascript
// Play a note: voice 0, note 69 (A4), velocity 1.0
fetch("http://localhost:60440/", {
  method: "POST",
  body: "v0 n69 a0 l1"
}).then(r => r.text()).then(console.log);

// Release the note after 500ms
setTimeout(() => {
  fetch("http://localhost:60440/", {
    method: "POST",
    body: "v0 l0"
  });
}, 500);
```

### Rust (`reqwest`)

```rust
use reqwest::blocking::Client;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new();
    let res = client.post("http://localhost:60440/-skode")
        .body("v0 w0 f440 a0 l1")
        .send()?
        .text()?;
    println!("Response: {}", res);
    Ok(())
}
```

### Go (`net/http`)

```go
package main

import (
	"fmt"
	"io"
	"net/http"
	"strings"
)

func main() {
	resp, err := http.Post("http://localhost:60440/", "text/plain", strings.NewReader("v0 w0 n60 a-3 l1"))
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	fmt.Printf("sk3 response: %s", body)
}
```

### Windows PowerShell

```powershell
Invoke-RestMethod -Uri "http://localhost:60440/" -Method Post -Body "v0 w0 f440 a0 l1"
```

### Netcat (`nc`) One-Shot

```bash
# Send single command and close connection
printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 17\r\n\r\nv0 w0 f440 a0 l1\n" | nc localhost 60440

# Query engine version
printf "GET /-version HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n" | nc localhost 60440
```

### `socat` One-Shot

```bash
# Send note trigger and print response
printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 17\r\n\r\nv0 w0 f440 a0 l1\n" | socat - TCP:localhost:60440
```

### Tcl One-Shot

```tcl
#!/usr/bin/env tclsh
# Using standard Tcl http package:
package require http

set tok [http::geturl "http://localhost:60440/-skode" -query "v0 w0 f440 a0 l1" -type "text/plain"]
puts "Response: [string trim [http::data $tok]]"
http::cleanup $tok
```

---

## 5. Continuous & Streaming Examples (`Connection: keep-alive`)

Holding the connection open preserves the connection's dedicated `skode_t` context. Variables (`=0,val`), register values (`$0`), and macro definitions (`[name]:...;`) created in earlier requests remain active across all subsequent requests on that socket until disconnected.

### A. Python Persistent Session (`requests.Session`)

```python
import requests
import time

# Create a persistent session (holds TCP connection and skode_t context open)
session = requests.Session()
url = "http://localhost:60440/"

# Step 1: Define a Skode macro in this connection's context
# In Skode, macros are defined with [name]:commands;
session.post(url, data="[lead]:v0 w0 f440 a-3 l1;")

# Step 2: Invoke the macro on the SAME connection (state is preserved!)
resp1 = session.post(url, data="lead")
print("Macro execution:", resp1.text)

time.sleep(0.5)

# Step 3: Change pitch and re-trigger over the same connection
session.post(url, data="v0 n64 l1")

# Step 4: Optional context reset without closing connection
session.post("http://localhost:60440/-reset")

# Step 5: Close connection -> socket closes, context is deterministically freed
session.close()
```

### B. Node.js Keep-Alive HTTP Agent

```javascript
import http from 'node:http';

const agent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
  keepAliveMsecs: 30000
});

function sendCommand(skodeLine) {
  return new Promise((resolve, reject) => {
    const req = http.request({
      hostname: 'localhost',
      port: 60440,
      path: '/-skode',
      method: 'POST',
      agent: agent,
      headers: {
        'Content-Type': 'text/plain',
        'Content-Length': Buffer.byteLength(skodeLine)
      }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve(data));
    });

    req.on('error', reject);
    req.write(skodeLine);
    req.end();
  });
}

async function runSession() {
  // 1. Define macro in this connection's context
  await sendCommand("[riff]:v0 n60 l1 v1 n67 l1;");

  // 2. Play riff repeatedly on the same persistent socket
  for (let i = 0; i < 4; i++) {
    console.log(await sendCommand("riff"));
    await new Promise(r => setTimeout(r, 200));
    await sendCommand("v0 l0 v1 l0"); // Release notes
    await new Promise(r => setTimeout(r, 100));
  }

  agent.destroy(); // Closes socket and releases context
}

runSession();
```

### C. Tcl Persistent Keep-Alive Session (Pure Native Sockets)

Tcl's native `socket` channel provides an ultra-lightweight client for continuous scripting without any external dependencies:

```tcl
#!/usr/bin/env tclsh

# Open persistent TCP connection to sk3 HTTP server
proc skode_open {host port} {
    set s [socket $host $port]
    fconfigure $s -translation binary -buffering line -blocking 1
    return $s
}

# Send command over open socket and read HTTP response body
proc skode_send {s cmd} {
    set len [string length $cmd]
    puts -nonewline $s "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: $len\r\n\r\n$cmd"
    flush $s

    # Parse HTTP response headers
    set content_len 0
    while {[gets $s line] >= 0} {
        set line [string trimright $line "\r\n"]
        if {$line eq ""} break
        if {[string match -nocase "content-length:*" $line]} {
            set content_len [string trim [lindex [split $line ":"] 1]]
        }
    }
    # Read exact payload body
    if {$content_len > 0} {
        return [read $s $content_len]
    }
    return ""
}

# Example live session:
set sock [skode_open localhost 60440]

# 1. Define chord macro in this connection's context
puts "Defined macro: [string trim [skode_send $sock {[c_major]:v0 n60 a-6 l1 v1 n64 a-6 l1 v2 n67 a-6 l1;}]]"

# 2. Invoke chord macro on the same socket (macro is preserved!)
puts "Triggered chord: [string trim [skode_send $sock {c_major}]]"

# 3. Query voice state
puts "Voice 0 status: [string trim [skode_send $sock {v0 ?}]]"

# 4. Close socket (context is freed cleanly)
close $sock
```

### D. `socat` Continuous Keep-Alive Pipeline

Stream multiple commands over a single persistent TCP connection:

```bash
(
  cmd1="[hit]:v0 w0 f440 a0 l1;"
  cmd2="hit"
  cmd3="v0 ?"
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n%s" "${#cmd1}" "$cmd1"
  sleep 0.1
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n%s" "${#cmd2}" "$cmd2"
  sleep 0.1
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: %d\r\n\r\n%s" "${#cmd3}" "$cmd3"
) | socat - TCP:localhost:60440
```

### E. Netcat (`nc`) Continuous Keep-Alive Pipeline

Identical pipelined streaming using standard Netcat:

```bash
(
  cmd1="[hit]:v0 w0 f440 a0 l1;"
  cmd2="hit"
  cmd3="v0 ?"
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n%s" "${#cmd1}" "$cmd1"
  sleep 0.1
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: %d\r\n\r\n%s" "${#cmd2}" "$cmd2"
  sleep 0.1
  printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: %d\r\n\r\n%s" "${#cmd3}" "$cmd3"
) | nc localhost 60440
```

### F. Persistent Interactive Unix TCP Pipe (`/dev/tcp`)

```bash
# Open interactive bidirectional TCP stream on file descriptor 3
exec 3<>/dev/tcp/127.0.0.1/60440

# Command 1: Define macro with keep-alive
printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: 26\r\n\r\n[hit]:v0 w0 f440 a0 l1;\n" >&3
head -n 7 <&3

# Command 2: Execute macro on the same socket
printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: 5\r\n\r\nhit\n" >&3
head -n 7 <&3

# Command 3: Query voice state and close connection
printf "POST /-skode HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 6\r\n\r\nv0 ?\n" >&3
cat <&3

# Close file descriptor
exec 3<&-
```

### G. Streaming a Complex Composition File

```bash
curl -X POST http://localhost:60440/-skode \
     -H "Connection: keep-alive" \
     --data-binary @track.sk
```

---

## 6. Browser Web App Communication Guide

### Can a Web App in My Browser Talk to `sk3` on `localhost`?

**Yes!** `sk3` is explicitly designed for direct browser-to-engine communication:

1. **Full CORS Headers**: Every response emitted by `sk3` includes:
   ```http
   Access-Control-Allow-Origin: *
   Access-Control-Allow-Methods: POST, OPTIONS, GET
   Access-Control-Allow-Headers: Content-Type, Authorization, X-Requested-With
   ```
2. **Private Network Access (PNA) Support**:
   Modern Chromium browsers (Chrome, Edge, Brave, Opera) implement Google's [Private Network Access](https://developer.chrome.com/blog/private-network-access-preflight/) specification when a public website communicates with `localhost`. `sk3` automatically replies to preflight requests with:
   ```http
   Access-Control-Allow-Private-Network: true
   ```
   This allows remote web editors (e.g. `https://octetta.github.io` or live notebooks) to talk directly to your local `sk3` instance without being blocked by browser private network security checks.
3. **Mixed Content Considerations**:
   - If your web app is hosted on `http://localhost:...` or `http://127.0.0.1:...` (e.g. Vite, Webpack dev server, Python `http.server`), HTTP-to-HTTP communication works seamlessly with zero warnings.
   - If your web app is loaded from `file:///...`, local loopback fetches to `http://localhost:60440` work seamlessly.
   - If your web app is served over public `https://...`, standard modern browsers permit communication with `http://localhost` because `localhost` is treated as a potentially trustworthy loopback origin. If a strict browser blocks the HTTP mixed content, running your web UI over `http://` or using a local dev proxy bypasses any mixed-content restrictions.
