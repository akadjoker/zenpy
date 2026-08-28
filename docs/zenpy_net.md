# Zen Net Library Reference

**Complete reference for TCP/UDP networking with 13 functions. POSIX sockets with no external dependencies.**

Import with: `import net`

---

## Overview

Low-level networking via BSD sockets. Supports TCP client/server and UDP communication.

### Quick Start

```python
import net

# TCP Client
sock = net.tcp_connect("example.com", 80)
net.send(sock, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")
response = net.recv(sock, 4096)
net.close(sock)

# TCP Server
server = net.tcp_listen(8000)
client = net.tcp_accept(server)
msg = net.recv(client)
net.send(client, "Hello!")
net.close(client)

# UDP
sock = net.udp_create(9000)
net.sendto(sock, "ping", "127.0.0.1", 9001)
pkt = net.recvfrom(sock)  # [data, ip, port]
```

| Function | Arity | Description |
|----------|-------|-------------|
| `resolve` | 1 | Resolve hostname to IP. |
| `tcp_connect` | 2 | Connect to TCP server. |
| `tcp_listen` | -1 | Create listening socket. |
| `tcp_accept` | 1 | Accept client connection. |
| `udp_create` | -1 | Create UDP socket. |
| `send` | 2 | Send data (TCP/UDP). |
| `recv` | -1 | Receive data (TCP/UDP). |
| `sendto` | 4 | Send UDP packet to address. |
| `recvfrom` | -1 | Receive UDP packet with source. |
| `set_blocking` | 2 | Set blocking/non-blocking. |
| `set_nodelay` | 2 | Enable TCP_NODELAY. |
| `poll` | 2 | Check if data available. |
| `close` | 1 | Close socket. |

---

## DNS & Connection

### resolve(hostname) → string | nil

Resolve hostname to IPv4 address.

**Signature:** `resolve(hostname: string) → string | nil`

**Returns:**
- IPv4 address string if found
- `nil` if cannot resolve

**Example:**
```python
import net

ip = net.resolve("example.com")
if ip != nil:
    print("IP:", ip)  # → "93.184.216.34"
    sock = net.tcp_connect(ip, 80)
else:
    print("Cannot resolve domain")
```

---

### tcp_connect(host, port) → int | nil

Connect to TCP server.

**Signature:** `tcp_connect(host: string, port: int) → int | nil`

**Returns:**
- Socket handle (int) on success
- `nil` on failure

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
if sock != nil:
    print("Connected, handle:", sock)
    net.close(sock)
else:
    print("Connection failed")
```

---

### tcp_listen(port[, backlog]) → int | nil

Create listening socket (server).

**Signature:** `tcp_listen(port: int[, backlog: int]) → int | nil`

**Parameters:**
- `port` — port to listen on
- `backlog` (optional) — connection queue size (default: 16)

**Returns:**
- Server socket handle on success
- `nil` on failure

**Example:**
```python
import net

server = net.tcp_listen(8000, 32)
if server != nil:
    print("Server listening on port 8000")
    client = net.tcp_accept(server)
    # ... handle client ...
    net.close(server)
else:
    print("Cannot create server")
```

---

### tcp_accept(server_handle) → int | nil

Accept incoming client connection.

**Signature:** `tcp_accept(server: int) → int | nil`

**Returns:**
- Client socket handle on success
- `nil` if no connection or error

**Note:** Blocks until client connects (use `poll()` for non-blocking)

**Example:**
```python
import net

server = net.tcp_listen(8000)

# Accept one client
client = net.tcp_accept(server)
if client != nil:
    data = net.recv(client)
    net.send(client, "Echo: " + data)
    net.close(client)

net.close(server)
```

---

## Data Transfer

### send(handle, data) → int

Send data over TCP socket.

**Signature:** `send(sock: int, data: string) → int`

**Returns:**
- Bytes sent (>= 0)
- -1 on error

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
request = "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n"
bytes_sent = net.send(sock, request)
print("Sent", bytes_sent, "bytes")
net.close(sock)
```

---

### recv(handle[, max_bytes]) → string | nil

Receive data from TCP socket.

**Signature:** `recv(sock: int[, max_bytes: int]) → string | nil`

**Parameters:**
- `max_bytes` (optional) — max bytes to receive (default: 4096, max: 16MB)

**Returns:**
- Data received (string) on success
- `nil` on error or connection closed

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
net.send(sock, "GET / HTTP/1.0\r\nHost: example.com\r\n\r\n")

response = net.recv(sock, 8192)
if response != nil:
    print("Received", len(response), "bytes")
    print(response[:100])

net.close(sock)
```

---

### udp_create([port]) → int | nil

Create UDP socket.

**Signature:** `udp_create([port: int]) → int | nil`

**Parameters:**
- `port` (optional) — port to bind. If omitted, system assigns.

**Returns:**
- Socket handle on success
- `nil` on failure

**Example:**
```python
import net

# Receive UDP packets on port 9000
sock = net.udp_create(9000)
if sock != nil:
    pkt = net.recvfrom(sock)  # [data, ip, port]
    print("From:", pkt[1], "port", pkt[2])
    net.close(sock)

# Send UDP without binding to port
send_sock = net.udp_create()
net.sendto(send_sock, "hello", "127.0.0.1", 9001)
```

---

### sendto(handle, data, host, port) → int

Send UDP packet to address.

**Signature:** `sendto(sock: int, data: string, host: string, port: int) → int`

**Returns:**
- Bytes sent (>= 0)
- -1 on error

**Example:**
```python
import net

sock = net.udp_create()
bytes_sent = net.sendto(sock, "ping", "127.0.0.1", 9001)
print("Sent", bytes_sent, "bytes via UDP")
net.close(sock)
```

---

### recvfrom(handle[, max_bytes]) → [data, ip, port] | nil

Receive UDP packet with source information.

**Signature:** `recvfrom(sock: int[, max_bytes: int]) → [data, ip, port] | nil`

**Returns:**
- Array `[data, ip, port]` on success
- `nil` on error

**Example:**
```python
import net

sock = net.udp_create(9000)
pkt = net.recvfrom(sock, 1024)

if pkt != nil:
    data = pkt[0]
    ip = pkt[1]
    port = pkt[2]
    print("UDP packet from", ip + ":" + str(port))
    print("Data:", data)

net.close(sock)
```

---

## Socket Control

### set_blocking(handle, blocking) → bool

Set socket to blocking or non-blocking mode.

**Signature:** `set_blocking(sock: int, blocking: bool) → bool`

**Parameters:**
- `blocking` — `true` for blocking (default), `false` for non-blocking

**Returns:** `true` if successful

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
net.set_blocking(sock, false)  // Non-blocking mode

// Check if data is available without blocking
if net.poll(sock, 100):  // 100ms timeout
    data = net.recv(sock)
else:
    print("No data yet")

net.close(sock)
```

---

### set_nodelay(handle, nodelay) → bool

Enable/disable TCP_NODELAY (Nagle's algorithm).

**Signature:** `set_nodelay(sock: int, nodelay: bool) → bool`

**Parameters:**
- `nodelay` — `true` to disable Nagle, `false` to enable

**Returns:** `true` if successful

**Note:** Disabling Nagle reduces latency at cost of more packets.

**Example:**
```python
import net

sock = net.tcp_connect("game-server.example.com", 9000)
net.set_nodelay(sock, true)  // Low-latency mode for gaming
net.send(sock, "player_move:x=100,y=200")
```

---

### poll(handle, timeout_ms) → bool

Check if socket has data available.

**Signature:** `poll(sock: int, timeout_ms: int) → bool`

**Parameters:**
- `timeout_ms` — timeout in milliseconds (0 for immediate check)

**Returns:**
- `true` if data available
- `false` if timeout or error

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
net.set_blocking(sock, false)

net.send(sock, "GET / HTTP/1.0\r\n\r\n")

// Wait up to 5 seconds for response
if net.poll(sock, 5000):
    response = net.recv(sock, 8192)
    print("Got response")
else:
    print("Timeout")

net.close(sock)
```

---

### close(handle) → bool

Close socket.

**Signature:** `close(sock: int) → bool`

**Returns:** `true` if successful

**Example:**
```python
import net

sock = net.tcp_connect("example.com", 80)
net.send(sock, "GET / HTTP/1.0\r\n\r\n")
data = net.recv(sock)
net.close(sock)
```

---

## Common Patterns

### HTTP Client

```python
import net

function http_get(host, path = "/"):
    sock = net.tcp_connect(host, 80)
    if sock == nil:
        return nil
    
    request = "GET " + path + " HTTP/1.0\r\nHost: " + host + "\r\nConnection: close\r\n\r\n"
    net.send(sock, request)
    
    response = ""
    while true:
        chunk = net.recv(sock, 4096)
        if chunk == nil:
            break
        response = response + chunk
    
    net.close(sock)
    return response

// Usage
html = http_get("example.com", "/index.html")
print(html[:200])
```

### Echo Server

```python
import net

function run_echo_server(port):
    server = net.tcp_listen(port)
    if server == nil:
        print("Cannot listen on port", port)
        return
    
    print("Echo server listening on port", port)
    
    while true:
        client = net.tcp_accept(server)
        if client != nil:
            data = net.recv(client)
            if data != nil:
                echo = "Echo: " + data
                net.send(client, echo)
            net.close(client)

// Usage
run_echo_server(9000)
```

### UDP Peer-to-Peer

```python
import net

class P2PPeer:
    def __init__(self, listen_port):
        self.sock = net.udp_create(listen_port)
        self.port = listen_port
    
    def send_to(self, peer_ip, peer_port, message):
        return net.sendto(self.sock, message, peer_ip, peer_port)
    
    def receive(self):
        pkt = net.recvfrom(self.sock)
        if pkt != nil:
            return {data: pkt[0], ip: pkt[1], port: pkt[2]}
        return nil
    
    def close(self):
        net.close(self.sock)

// Usage
peer = P2PPeer(9000)
peer.send_to("127.0.0.1", 9001, "hello")
msg = peer.receive()
if msg != nil:
    print("Got:", msg["data"], "from", msg["ip"])
```

### Non-Blocking Client

```python
import net
import time

function connect_with_timeout(host, port, timeout_sec):
    sock = net.tcp_connect(host, port)
    if sock == nil:
        return nil
    
    net.set_blocking(sock, false)
    
    request = "GET / HTTP/1.0\r\nHost: " + host + "\r\n\r\n"
    net.send(sock, request)
    
    start = time.monotonic()
    while true:
        if net.poll(sock, 100):  // 100ms poll
            data = net.recv(sock, 8192)
            if data == nil:
                break
            return data
        
        elapsed = time.monotonic() - start
        if elapsed > timeout_sec:
            return nil

// Usage
result = connect_with_timeout("example.com", 80, 10)
if result != nil:
    print("Got response")
```

### Broadcast Listener

```python
import net

function listen_udp_broadcast(port):
    sock = net.udp_create(port)
    if sock == nil:
        return
    
    while true:
        pkt = net.recvfrom(sock, 1024)
        if pkt != nil:
            message = pkt[0]
            from_ip = pkt[1]
            from_port = pkt[2]
            process_broadcast(message, from_ip, from_port)

def process_broadcast(msg, ip, port):
    print("Broadcast from", ip + ":" + str(port))
    print("Message:", msg)
```

---

## Limits

- Maximum sockets: 64
- Maximum receive: 16 MB per call
- UDP payload: 65536 bytes (system dependent)
- Blocking socket: blocks indefinitely (use `poll()` for timeout)

---

## Error Handling

```python
import net

// Connection errors
sock = net.tcp_connect("invalid-host.example.com", 80)
if sock == nil:
    print("Connection failed")

// Send/recv errors
if sock != nil:
    bytes_sent = net.send(sock, "data")
    if bytes_sent < 0:
        print("Send failed")
    
    data = net.recv(sock, 1024)
    if data == nil:
        print("Recv failed or connection closed")
```

---

## Platform Notes

- **Linux/macOS:** Full POSIX socket support
- **Windows:** Requires WinSock2 (automatically initialized)
- **IPv4 only:** No IPv6 support yet
- **No SSL/TLS:** Use plaintext protocols or external library

---

**Generated from:** `libzen/src/builtin_net.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 13 functions (DNS, TCP client/server, UDP, socket control)
