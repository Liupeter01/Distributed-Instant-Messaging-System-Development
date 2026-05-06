#!/bin/bash
set -e

echo "=== macOS Temporary High-Concurrency Limit Setup (run before each test) ==="

echo "[0/4] Current limits (before changes):"
echo "  ulimit soft: $(ulimit -n)"
echo "  ulimit hard: $(ulimit -Hn)"
echo "  launchctl  : $(launchctl limit maxfiles | awk '{print $2 " / " $3}')"

echo "[1/4] 1. Raising launchd file descriptor limit (affects new processes)..."
sudo launchctl limit maxfiles 65536 524288

echo "[2/4] 2. Raising current shell soft and hard limits..."
ulimit -Hn 65536   # set hard limit first
ulimit -n 65536    # then set soft limit

echo "[3/4] 3. Temporarily expanding TCP ephemeral port range..."
sudo sysctl -w net.inet.ip.portrange.first=10000
sudo sysctl -w net.inet.ip.portrange.last=65535

echo "[4/4] 4. Setup complete! Final limits:"
echo "  ulimit soft: $(ulimit -n)"
echo "  ulimit hard: $(ulimit -Hn)"
echo "  launchctl  : $(launchctl limit maxfiles | awk '{print $2 " / " $3}')"

echo ""
echo "  Temporary limits applied successfully!"
echo "⚠️ Important:"
echo "   Run your test program in **this exact same Terminal window**!"
echo "   (Do NOT open a new window or use IDE 'Run' button — it won't inherit the new limits.)"
echo "   When you close this Terminal, everything returns to normal automatically."