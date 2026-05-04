# Im-benchmark

Stand-alone stress benchmarks for the Distributed IM System's TCP long-connection layer.

## 0x00 About Im-benchmark

### 1. About this project

#### 1.1 Introduction

This project is **protocol-compatible** with the production IM server's TLV framing (2B msg_id + 2B total_length, Big Endian) but intentionally bypasses all business logic (`Redis`, `MySQL`, `gRPC`) to isolate connection-layer throughput.

#### 1.2 Prerequisites

- **C++17** compiler (MSVC / GCC / Clang)
- ~~**Boost** (system component — for Boost.Asio)：auto downloaded by FetchContent~~
- **CMake** 3.16+
- **Python 3** with `numpy` (for `analyze_rtt.py`)

No other dependencies. No TBB, no Qt, no gRPC, no Redis, no MySQL.



#### 1.3 Project Structure

```scss
im-benchmark/
└── im-bench-client/
│   ├── include/
│   │   ├── def.hpp
│   │   ├── protocol.h			# Minimal TLV protocol (extracted from production msgnode.hpp)
│   │   └── echo_client.h
│   └── src/
│       ├── echo_client.cpp
│       └── main.cpp
└─im-bench-server
│   ├── include/
│   │   ├── def.hpp
│   │   ├── protocol.h			# Minimal TLV protocol (extracted from production msgnode.hpp)
│   │   ├── echo_session.h
│   │   └── echo_server.h
│   └── src/
│       ├── echo_server.cpp 	# Benchmark-only echo server (no business logic)
│       ├── echo_session.cpp
│       └── main.cpp
├── CMakeLists.txt          	# Boost.Asio only — no TBB, Qt, gRPC, Redis
└── README.md
├── analyze_rtt.py          # Post-processing: percentiles, WiFi jitter detection
└── results/
    ├── rtt_1k.csv
    └── rtt_10k.csv
```



### 2. What Is Measured

The echo server receives a TLV-framed echo and echoes the packet back unchanged. The stress client embeds a `uint64_t` microsecond timestamp in the payload, and computes round-trip time on receipt.

#### 2.1 Why a separate echo server instead of adding a benchmark mode to the production server?

The production server depends on Redis, MySQL, gRPC, ServerConfig, UserManager, and SyncLogic. Pulling in these dependencies would mean either:

1. Starting the full stack for a connection-layer benchmark (bottleneck shifts to DB), or
2. Stubbing out every dependency (fragile, hard to maintain).

A standalone echo server with protocol-compatible TLV framing gives clean, reproducible numbers that reflect **only** the Asio + TCP + framing cost. The `#ifdef`-guarded approach was considered but rejected in favor of full isolation for build independence (the benchmark compiles on macOS where the production server's TBB dependency currently fails).



#### 2.2 Protocol Compatibility

The TLV frame format used by `protocol.hpp` is **byte-level identical** to the production server's `MsgHeader` / `SendNode` / `RecvNode` (see `msgnode.hpp` in the main IM repository). The echo server's read path (`read_header` → `read_body` → `send_echo`) mirrors the production `Session::handle_header` → `handle_msgbody` flow, minus business dispatch.

#### 2.3 WiFi Path Disclaimer

When the client runs over WiFi (e.g., Mac WiFi → Windows wired server), end-to-end RTT includes WLAN latency, 802.11 retransmissions, and periodic WiFi scan stalls. The `analyze_rtt.py` script isolates these artifacts by detecting p99 spikes in 10-second windows. Results are reported as both **raw** (full distribution) and **clean** (WiFi spikes filtered).

Server-side metrics (CPU%, RSS) are unaffected by client network path and reflect true connection-layer cost.

#### 2.4 What This Benchmark Does NOT Measure

- Gateway HTTP latency (login / auth path)
- gRPC inter-service RPC latency
- MySQL query latency or Redis cache performance
- End-to-end message delivery through the full service mesh
- TLS/SSL handshake overhead (benchmark runs unencrypted)

These are intentionally excluded to isolate the TCP connection layer. Full-path benchmarks would require the complete service stack and are out of scope for this harness.

| Component under test                            | What is bypassed               |
| ----------------------------------------------- | ------------------------------ |
| TCP accept / connection management (Boost.Asio) | Gateway HTTP path              |
| TLV header parsing + body framing               | gRPC service mesh              |
| Async write-back (echo)                         | MySQL / Redis persistence      |
| Concurrent session scaling                      | Authentication / session state |



## 0x01 Developer Quick Start

### 1. How to Compile Both

```c++
cd Distributed-Instant-Messaging-System-Development
cmake -Bbuild -DCMAKE_BUILD_TYPE=Release -DDIMS_ENABLE_BENCH_ECHO=ON -DCMAKE_CXX_FLAGS=-03
cmake --build build --parallel [x]
```



### 2. System Tuning

#### 2.1 Windows Server(windows_bench_setup.ps1)

```powershell
# Expand dynamic port range (run as Administrator)
netsh int ipv4 set dynamicport tcp start=10000 num=55000

# Allow inbound connections on benchmark port
New-NetFirewallRule -DisplayName "IM Bench" -Direction Inbound -LocalPort 8888 -Protocol TCP -Action Allow
```

#### 2.2 Linux (linux_bench_setup.sh)

```bash
ulimit -n 65536
sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535
sudo sysctl -w net.ipv4.ip_local_port_range="10000 65535"
sudo sysctl -w net.ipv4.tcp_tw_reuse=1
```

#### 2.3 MacOS Client(macos_bench_setup.sh)

```bash
# Raise file descriptor limit
ulimit -n 65536

# If the above fails:
sudo launchctl limit maxfiles 65536 200000

# Expand local port range
sudo sysctl -w net.inet.ip.portrange.first=10000
sudo sysctl -w net.inet.ip.portrange.last=65535
```



### 3. Execute Performance Test

#### 3.1 Run im-bench-server

```bash
# Usage: im-bench-server <port>
./im-bench-server 8888
```

#### 3.2 Run im-bench-client

Each run sustains connections for **5 minutes** (300 seconds), sending 1 ping/sec/connection.

> **Important:** Wait ~60 seconds between runs to let TIME_WAIT sockets expire.

```bash
# Usage: im-bench-client <server_ip> <port> <num_conns> <output_csv>
./im-bench-client 192.168.1.100 8888 1000  results/rtt_1k.csv
./im-bench-client 192.168.1.100 8888 2500  results/rtt_2_5k.csv
./im-bench-client 192.168.1.100 8888 5000  results/rtt_5k.csv
./im-bench-client 192.168.1.100 8888 7500  results/rtt_7_5k.csv
./im-bench-client 192.168.1.100 8888 10000 results/rtt_10k.csv
```



## 0x02 Data Analysis

### 1. Post-Processing

#### 1.1 About Data Analysis Method

The script reports:

- Overall p50 / p99 / max RTT per load level
- Per-10-second-window p99 analysis
- **WiFi jitter spike detection** — windows where p99 exceeds 3× the median window p99 are flagged as network artifacts
- **Clean p50 / p99** — percentiles after filtering jitter-affected windows



#### ~~1.2 analysis RTT script(Not Finished)~~

```bash
python3 analyze_rtt.py results/rtt_1k.csv results/rtt_5k.csv results/rtt_10k.csv
```



### ~~2. Example Results(Not Finished)~~

```
N_conns | conn_ok% | p50_us (raw) | p99_us (raw) | p99_us (clean) | CPU% | RSS_MB
   1000 |   99.98  |          ... |          ... |            ... |  ... |    ...
   5000 |   99.90  |          ... |          ... |            ... |  ... |    ...
  10000 |   99.80  |          ... |          ... |            ... |  ... |    ...
```



## License

Same license as the parent IM System repository.