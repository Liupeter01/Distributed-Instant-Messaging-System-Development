#!/bin/bash

# =========================
# IM Benchmark macOS Setup
# =========================

set -e

echo "[1/3] Raising file descriptor limit (current session)..."
ulimit -n 65536 || echo "Warning: ulimit change limited on macOS"

echo "[2/3] Setting launchctl maxfiles (persistent attempt)..."
sudo launchctl limit maxfiles 65536 200000

echo "[3/3] Expanding TCP port range..."
sudo sysctl -w net.inet.ip.portrange.first=10000
sudo sysctl -w net.inet.ip.portrange.last=65535

echo "Done."

