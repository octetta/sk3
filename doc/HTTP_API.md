# sk3 HTTP Control API Reference

`sk3` features a built-in, zero-dependency, multiplexed HTTP control server. It enables web browsers, command-line scripts, language runtimes, and external DAWs to evaluate Forth/Skode commands over standard HTTP POST requests without needing raw UDP, binary OSC, or WebSocket daemons.

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
  - Definitions, variables, voice assignments, and macro expansions created by one connection remain completely isolated from other connections.
  - When the client disconnects, `skode_free` is automatically invoked, releasing all memory deterministically with zero leaks.
* **Continuous Connection Support (`Keep-Alive`)**:
  - Full HTTP/1.1 `Connection: keep-alive` support with a 60-second idle timeout.
  - Clients can keep the socket open and stream back-to-back command payloads; all state defined earlier in the session remains alive until disconnection.
* **CORS Support**:
  - Built-in `Access-Control-Allow-Origin: *` headers on all responses, with automatic handling of `OPTIONS` preflight requests (204 No Content), allowing web apps and notebooks to control `sk3` directly from any domain.
* **WASM Safety**:
  - Completely stubbed out at compile time in WebAssembly builds (`__EMSCRIPTEN__` and `SK_NO_NETWORK`).

---

## 2. Endpoints & URL Paths

Clients can use any of the following standard paths while experimenting:

| Method | Path | Content-Type | Description |
| :--- | :--- | :--- | :--- |
| `POST` | `/` | `text/plain` | Evaluates Skode payload in connection context. Returns stdout log or `OK\n`. |
| `POST` | `/skode` | `text/plain` | Explicit endpoint for Skode evaluation (synonymous with `/`). |
| `POST` | `/cmd` or `/api` | `text/plain` | Convenient REST aliases for command execution. |
| `GET` | `/` | `text/plain` | Health check banner with available endpoints and version. |
| `GET` | `/status` | `application/json` | Live JSON health check and HTTP server metrics. |
| `GET` | `/metrics` | `application/json` | Synonymous with `/status`. |
| `GET` | `/version` | `text/plain` | Returns current engine version string (e.g. `1.3.20\n`). |
| `OPTIONS`| `*` | *(none)* | CORS preflight: returns HTTP 204 No Content. |

---

## 3. One-Shot Examples (Fire-and-Forget)

One-shot requests establish a connection, send a command, receive the response, and terminate (`Connection: close`).

### cURL (Bash / Terminal)

```bash
# Basic synthesizer voice trigger
curl -X POST http://localhost:60440/ -d "v0 wf=0 f=440 a=0.8"

# Inspect engine opcodes or voice state
curl -X POST http://localhost:60440/skode -d "??"

# Execute an entire script file from disk
curl -X POST http://localhost:60440/ --data-binary @track.sk

# Read current engine version
curl http://localhost:60440/version

# Query live metrics in JSON format
curl -s http://localhost:60440/status
```

### Python (`requests`)

```python
import requests

url = "http://localhost:60440/"

# Set voice 0 to sawtooth, 440 Hz, trigger amplitude 0.85
response = requests.post(url, data="v0 wf=0 f=440 a=0.85")
print(response.text) # Returns "OK\n" or captured log

# Query status metrics
status = requests.get("http://localhost:60440/status").json()
print("Connected to sk3 version:", status["version"])
```

### Node.js (Native `fetch`)

```javascript
// Trigger physical plucked string (440 Hz, vel 0.8)
const res = await fetch("http://localhost:60440/", {
  method: "POST",
  body: "v0 440 0.8 0.2 pluck"
});
console.log(await res.text());
```

### Web Browser (Client-Side JavaScript)

Because `sk3` emits CORS headers, you can send commands directly from the Chrome/Firefox/Safari developer console or any web application:

```javascript
fetch("http://localhost:60440/skode", {
  method: "POST",
  headers: { "Content-Type": "text/plain" },
  body: "v0 wf=1 f=220 a=0.9"
})
.then(r => r.text())
.then(console.log);
```

### Rust (`reqwest`)

```rust
use reqwest::blocking::Client;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new();
    let res = client.post("http://localhost:60440/")
        .body("v0 wf=0 f=440 a=0.8")
        .send()?
        .text()?;
    println!("sk3 response: {}", res);
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
	resp, err := http.Post("http://localhost:60440/", "text/plain", strings.NewReader("v0 wf=0 f=440 a=0.8"))
	if err != nil {
		panic(err)
	}
	defer resp.Body.Close()
	body, _ := io.ReadAll(resp.Body)
	fmt.Printf("Response: %s", body)
}
```

### Windows PowerShell

```powershell
Invoke-RestMethod -Uri "http://localhost:60440/" -Method Post -Body "v0 wf=0 f=440 a=0.8"
```

---

## 4. Continuous & Streaming Examples (`Connection: keep-alive`)

Holding the connection open preserves the connection's dedicated `skode_t` context. Variables and words defined in earlier requests remain accessible across all subsequent requests on that socket until disconnected.

### A. Python Persistent Session (`requests.Session`)

```python
import requests
import time

# Create a persistent session (holds TCP connection and skode_t context open)
session = requests.Session()
url = "http://localhost:60440/"

# Step 1: Define a custom Forth word in this connection's context
session.post(url, data=": my_synth 440 f= 0.8 a= ;")

# Step 2: Call the defined word on the SAME connection (state is preserved!)
resp1 = session.post(url, data="v0 my_synth")
print("Response 1:", resp1.text)

time.sleep(0.5)

# Step 3: Trigger another voice using the session
resp2 = session.post(url, data="v1 880 f= 0.5 a=")
print("Response 2:", resp2.text)

# Step 4: Close session -> socket closes, skode_t context is cleanly freed
session.close()
```

### B. Node.js Keep-Alive HTTP Agent

```javascript
import http from 'node:http';

const keepAliveAgent = new http.Agent({
  keepAlive: true,
  maxSockets: 1,
  keepAliveMsecs: 30000
});

function sendCommand(cmd) {
  return new Promise((resolve, reject) => {
    const req = http.request({
      hostname: 'localhost',
      port: 60440,
      path: '/skode',
      method: 'POST',
      agent: keepAliveAgent,
      headers: {
        'Content-Type': 'text/plain',
        'Content-Length': Buffer.byteLength(cmd)
      }
    }, (res) => {
      let data = '';
      res.on('data', chunk => data += chunk);
      res.on('end', () => resolve(data));
    });

    req.on('error', reject);
    req.write(cmd);
    req.end();
  });
}

async function runLiveSession() {
  // Define a sequence
  await sendCommand(": hit 440 f= 0.8 a= ;");
  // Execute repeatedly on the same open socket
  for (let i = 0; i < 4; i++) {
    console.log(await sendCommand("v0 hit"));
    await new Promise(r => setTimeout(r, 250));
  }
  keepAliveAgent.destroy(); // Tears down connection and frees context
}

runLiveSession();
```

### C. Persistent Interactive Netcat / Raw TCP Pipe

Because HTTP/1.1 is plain text, you can drive `sk3` interactively over a raw socket using standard Unix pipeline tools:

```bash
# Open interactive bidirectional TCP stream on file descriptor 3
exec 3<>/dev/tcp/127.0.0.1/60440

# Send Command 1 with keep-alive
printf "POST / HTTP/1.1\r\nHost: localhost\r\nConnection: keep-alive\r\nContent-Length: 21\r\n\r\n: arp 440 f= 0.8 a= ;\n" >&3

# Read response from fd 3
head -n 7 <&3

# Send Command 2 (executing word defined in Command 1)
printf "POST / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\nContent-Length: 7\r\n\r\nv0 arp\n" >&3

# Read response until socket terminates
cat <&3

# Close file descriptor
exec 3<&-
```

### D. Streaming Continuous Skode Script from Disk

To execute large multi-bar musical compositions containing hundreds of lines:

```bash
curl -X POST http://localhost:60440/skode \
     -H "Connection: keep-alive" \
     -H "Content-Type: text/plain" \
     --data-binary @track.sk
```

Each line in `track.sk` is processed sequentially within the isolated connection context, executing opcodes sample-accurately and returning any diagnostic output upon completion.
