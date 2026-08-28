# Zen HTTP Library Reference

**Complete reference for HTTP client operations with 5 functions. No HTTPS (yet), no external dependencies.**

Import with: `import http`

---

## Overview

High-level HTTP client built on POSIX sockets. Supports GET, POST, downloads, and connection checks.

| Function | Arity | Description |
|----------|-------|-------------|
| `get` | -1 | HTTP GET request. `get(url[, timeout])` |
| `post` | -1 | HTTP POST request. `post(url, body[, content_type][, timeout])` |
| `download` | -1 | Download file. `download(url, filepath[, timeout])` |
| `ping` | -1 | Check connection. `ping(host[, port][, timeout])` |
| `get_local_ip` | 0 | Get local IP address. |

---

## get(url[, timeout]) → map

Make HTTP GET request.

**Signature:** `get(url: string[, timeout: int]) → map`

**Returns:** Response map with keys:
- `status` (int) — HTTP status code (200, 404, etc.)
- `body` (string) — Response body
- `headers` (map) — Response headers
- `success` (bool) — true if 200-299
- `url` (string) — Original URL

**Timeout:** Default 30 seconds

**Example:**
```python
import http
import json

# Simple GET
r = http.get("http://api.example.com/users")
if r["success"]:
    data = json.parse(r["body"])
    print("Users:", data)
else:
    print("Error:", r["status"])

# With custom timeout
r = http.get("http://slow-api.example.com", 60)
if r["status"] == 200:
    print(r["body"])

# Check status codes
r = http.get("http://example.com")
if r["status"] == 404:
    print("Not found")
elif r["status"] == 500:
    print("Server error")
elif r["success"]:
    print("OK:", r["body"][:100])

# Access headers
r = http.get("http://example.com")
content_type = r["headers"]["Content-Type"]
print("Type:", content_type)
```

---

## post(url, body[, content_type][, timeout]) → map

Make HTTP POST request.

**Signature:** `post(url: string, body: string[, content_type: string][, timeout: int]) → map`

**Parameters:**
- `url` — target URL
- `body` — request body (string)
- `content_type` (optional) — MIME type (default: "application/x-www-form-urlencoded")
- `timeout` (optional) — timeout in seconds (default: 30)

**Common content types:**
- `"application/json"` — JSON data
- `"application/x-www-form-urlencoded"` — form data
- `"text/plain"` — plain text

**Returns:** Response map (same format as GET)

**Example:**
```python
import http
import json

# JSON POST
data = {name: "Alice", age: 30}
r = http.post("http://api.example.com/users", 
              json.stringify(data), 
              "application/json")
if r["success"]:
    print("Created!")
else:
    print("Error:", r["status"])

# Form POST
form_data = "username=alice&password=secret"
r = http.post("http://example.com/login", form_data)
if r["status"] == 200:
    print("Logged in!")

# Plain text
r = http.post("http://log-server.example.com/logs",
              "Application started\n",
              "text/plain")

# No content type (default)
r = http.post("http://example.com/submit", "param1=value1&param2=value2")
```

---

## download(url, filepath[, timeout]) → bool

Download file from URL and save to disk.

**Signature:** `download(url: string, filepath: string[, timeout: int]) → bool`

**Parameters:**
- `url` — file URL
- `filepath` — local path to save
- `timeout` (optional) — timeout in seconds (default: 60)

**Returns:**
- `true` if download successful
- `false` if failed

**Example:**
```python
import http
import io
import os

# Simple download
if http.download("http://example.com/file.zip", "./file.zip"):
    print("Downloaded!")
    size = io.size("./file.zip")
    print("Size:", size, "bytes")
else:
    print("Download failed")

# Download with error handling
urls = [
    "http://example.com/data.json",
    "http://backup.example.com/data.json"
]

for url in urls:
    if http.download(url, "data.json", 30):
        print("Downloaded from:", url)
        break
else:
    print("All downloads failed")

# Download to temp, then process
if http.download("http://example.com/archive.tar.gz", "/tmp/archive.tar.gz"):
    content = io.read("/tmp/archive.tar.gz")
    print("Downloaded", len(content), "bytes")
```

---

## ping(host[, port][, timeout]) → bool

Check if host is reachable.

**Signature:** `ping(host: string[, port: int][, timeout: int]) → bool`

**Parameters:**
- `host` — hostname or IP address
- `port` (optional) — port number (default: 80)
- `timeout` (optional) — timeout in seconds (default: 2)

**Returns:**
- `true` if connection successful
- `false` if unreachable

**Note:** This is TCP ping, not ICMP ping (requires open port)

**Example:**
```python
import http

# Check if web server is up
if http.ping("example.com"):
    print("Website is up!")
else:
    print("Website is down")

# Check specific service
if http.ping("api.example.com", 8080, 3):
    print("API server is running")
else:
    print("API server is down")

# Check multiple servers
servers = [
    ["primary.example.com", 80],
    ["backup.example.com", 80]
]

for srv in servers:
    if http.ping(srv[0], srv[1], 2):
        print("Using:", srv[0])
        break

# Network availability check
if http.ping("8.8.8.8", 53, 5):
    print("Network is available")
else:
    print("No internet connection")
```

---

## get_local_ip() → string | nil

Get local machine's IP address.

**Signature:** `get_local_ip() → string | nil`

**Returns:**
- Local IP address (string) if available
- `nil` if cannot determine

**Example:**
```python
import http

ip = http.get_local_ip()
if ip != nil:
    print("Local IP:", ip)
else:
    print("Cannot determine IP")

# Use in server startup
def start_server(port):
    ip = http.get_local_ip()
    print("Server running at http://" + ip + ":" + str(port))
    # ... start server ...
```

---

## Response Map Format

### Structure

All `get()` and `post()` return a map:

```python
{
    "status":  200,
    "body":    "<html>...",
    "headers": {
        "Content-Type": "text/html",
        "Content-Length": "1234",
        ...
    },
    "success": true,
    "url":     "http://example.com"
}
```

### Keys

| Key | Type | Description |
|-----|------|-------------|
| `status` | int | HTTP status code |
| `body` | string | Response body |
| `headers` | map | HTTP response headers |
| `success` | bool | true if 200-299 range |
| `url` | string | Original request URL |

### Example

```python
import http
import json

r = http.get("http://jsonapi.example.com/data")

print("Status:", r["status"])
print("Success:", r["success"])
print("Content type:", r["headers"]["Content-Type"])
print("Body preview:", r["body"][:100])

// Parse JSON response
if r["success"] and r["headers"]["Content-Type"].contains("json"):
    data = json.parse(r["body"])
    print("Data:", data)
```

---

## Common Patterns

### API Client

```python
import http
import json

class APIClient:
    def __init__(self, base_url, timeout = 30):
        self.base_url = base_url
        self.timeout = timeout
    
    def get(self, endpoint):
        url = self.base_url + endpoint
        r = http.get(url, self.timeout)
        if not r["success"]:
            print("API error:", r["status"])
            return nil
        return json.parse(r["body"])
    
    def post(self, endpoint, data):
        url = self.base_url + endpoint
        body = json.stringify(data)
        r = http.post(url, body, "application/json", self.timeout)
        if not r["success"]:
            print("API error:", r["status"])
            return nil
        return json.parse(r["body"])

# Usage
api = APIClient("http://api.example.com", 30)
users = api.get("/users")
new_user = api.post("/users", {name: "Alice", email: "alice@example.com"})
```

### Retry Logic

```python
import http
import time

function http_get_with_retry(url, max_retries = 3, timeout = 10):
    for attempt in range(max_retries):
        r = http.get(url, timeout)
        if r["success"]:
            return r
        
        print("Attempt", attempt + 1, "failed, retrying...")
        if attempt < max_retries - 1:
            time.sleep(2 ^ attempt)  // Exponential backoff
    
    return nil

// Usage
response = http_get_with_retry("http://api.example.com/data")
if response != nil:
    print("Success:", response["body"])
else:
    print("Failed after retries")
```

### Health Check

```python
import http
import time

class HealthCheck:
    def __init__(self, services):
        self.services = services  // [{name, host, port}]
    
    def check(self):
        results = {}
        for svc in self.services:
            is_up = http.ping(svc["host"], svc["port"], 2)
            results[svc["name"]] = is_up
        return results
    
    def report(self):
        status = self.check()
        for name in status:
            print(name + ":", "UP" if status[name] else "DOWN")

// Usage
check = HealthCheck([
    {name: "Web", host: "example.com", port: 80},
    {name: "API", host: "api.example.com", port: 8080},
    {name: "DB", host: "db.example.com", port: 5432}
])
check.report()
```

### File Synchronization

```python
import http
import io
import json

class FileSync:
    def __init__(self, server_url):
        self.server = server_url
    
    def upload(self, local_file):
        content = io.read(local_file)
        r = http.post(self.server + "/upload",
                     content,
                     "application/octet-stream")
        return r["success"]
    
    def download(self, remote_name, local_path):
        url = self.server + "/download?file=" + remote_name
        return http.download(url, local_path)

// Usage
sync = FileSync("http://server.example.com")
if sync.upload("config.json"):
    print("Uploaded!")
if sync.download("config.json", "config.json.new"):
    print("Downloaded!")
```

### JSON Data Fetcher

```python
import http
import json

class JSONFetcher:
    def __init__(self, base_url):
        self.base_url = base_url
    
    def fetch(self, endpoint):
        url = self.base_url + endpoint
        r = http.get(url)
        
        if not r["success"]:
            print("HTTP error:", r["status"])
            return nil
        
        try:
            return json.parse(r["body"])
        except:
            print("JSON parse error")
            return nil

// Usage
fetcher = JSONFetcher("http://api.example.com")
data = fetcher.fetch("/v1/config")
if data != nil:
    print("Config:", data)
```

---

## Status Codes

| Code | Meaning |
|------|---------|
| 200 | OK (success) |
| 201 | Created |
| 204 | No Content |
| 301, 302 | Redirect (not followed) |
| 400 | Bad Request |
| 401 | Unauthorized |
| 403 | Forbidden |
| 404 | Not Found |
| 500 | Internal Server Error |
| 503 | Service Unavailable |

---

## Limitations & Notes

- **HTTP only:** No HTTPS support (yet)
- **No redirects:** 3xx responses not automatically followed
- **Max response:** 16 MB per request
- **No cookies:** Headers included but no cookie jar
- **No authentication:** No built-in auth (pass in Authorization header manually)
- **Blocking:** All requests block until complete (no async)
- **No compression:** gzip/deflate not automatically handled

---

## Error Handling

```python
import http

// Connection errors return nil or empty response
r = http.get("http://invalid-domain-xyz.com")
if r == nil:
    print("Connection failed")
    return

// HTTP errors don't throw, check success flag
r = http.post("http://example.com/submit", "data")
if not r["success"]:
    print("HTTP error:", r["status"])
    print("Body:", r["body"][:100])
```

---

**Generated from:** `libzen/src/builtin_http.cpp`  
**Last Updated:** 2026-05-18  
**Coverage:** 5 functions (get, post, download, ping, get_local_ip)
