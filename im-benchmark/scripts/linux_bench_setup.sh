#!/bin/bash

# =========================
# IM Benchmark Linux Setup
# =========================

set -e

echo "[1/5] Raising file descriptor limit..."
ulimit -n 65536 || echo "Warning: ulimit change failed (may need /etc/security/limits.conf)"

echo "[2/5] sysctl tuning..."

sudo sysctl -w net.core.somaxconn=65535
sudo sysctl -w net.ipv4.tcp_max_syn_backlog=65535

echo "[3/5] expanding local port range..."
sudo sysctl -w net.ipv4.ip_local_port_range="10000 65535"

echo "[4/5] enabling TIME_WAIT reuse..."
sudo sysctl -w net.ipv4.tcp_tw_reuse=1

echo "[5/5] done"

