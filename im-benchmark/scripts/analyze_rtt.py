#!/usr/bin/env python3
"""
analyze_rtt.py ¡ª Post-processing for IM Bench RTT data.

Reads CSV files produced by im-bench-client (columns: t_ms, rtt_us),
generates per-file summary statistics, identifies WiFi/OS jitter windows,
and reports clean vs raw percentiles.

Usage:
    python3 analyze_rtt.py results/rtt_1k.csv results/rtt_5k.csv ...
    python3 analyze_rtt.py results/rtt_*.csv
"""

import sys
import csv
import os
import math
from collections import defaultdict


def load_csv(path):
    """Load t_ms and rtt_us columns from a CSV file."""
    t_ms = []
    rtt_us = []
    with open(path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            t_ms.append(int(row["t_ms"]))
            rtt_us.append(float(row["rtt_us"]))
    return t_ms, rtt_us


def percentile(sorted_data, p):
    """Return the p-th percentile from sorted data (0-100)."""
    if not sorted_data:
        return 0.0
    idx = min(int(len(sorted_data) * p / 100), len(sorted_data) - 1)
    return sorted_data[idx]


def format_us(us):
    """Format microseconds into a human-readable string."""
    if us < 1000:
        return f"{us:.0f} ¦Ìs"
    elif us < 1_000_000:
        return f"{us / 1000:.2f} ms"
    else:
        return f"{us / 1_000_000:.2f} s"


def analyze_file(path):
    """Analyze a single CSV file and print results."""
    basename = os.path.basename(path)
    t_ms, rtt_us = load_csv(path)

    if not rtt_us:
        print(f"\n{'=' * 60}")
        print(f"  {basename}: EMPTY ¡ª no samples")
        print(f"{'=' * 60}")
        return None

    n = len(rtt_us)
    sorted_rtt = sorted(rtt_us)

    # --- Overall stats ---
    p50 = percentile(sorted_rtt, 50)
    p90 = percentile(sorted_rtt, 90)
    p99 = percentile(sorted_rtt, 99)
    p999 = percentile(sorted_rtt, 99.9)
    rtt_min = sorted_rtt[0]
    rtt_max = sorted_rtt[-1]
    rtt_mean = sum(rtt_us) / n

    # --- Duration ---
    if t_ms:
        duration_s = (max(t_ms) - min(t_ms)) / 1000.0
    else:
        duration_s = 0

    # --- Windowed analysis (10-second buckets) ---
    bucket_ms = 10_000
    buckets = defaultdict(list)
    for t, rtt in zip(t_ms, rtt_us):
        bucket_id = t // bucket_ms
        buckets[bucket_id].append(rtt)

    window_p99s = []
    window_medians = []
    for bid in sorted(buckets.keys()):
        vals = sorted(buckets[bid])
        if len(vals) >= 10:
            window_p99s.append((bid, percentile(vals, 99)))
            window_medians.append((bid, percentile(vals, 50)))

    # --- Jitter detection ---
    # A window is "jittery" if its p99 > 3¡Á the median of all window p99s
    if window_p99s:
        all_wp99 = sorted([wp for _, wp in window_p99s])
        median_wp99 = percentile(all_wp99, 50)
        jitter_threshold = median_wp99 * 3

        jitter_windows = [(bid, wp) for bid, wp in window_p99s if wp > jitter_threshold]
        clean_windows = [(bid, wp) for bid, wp in window_p99s if wp <= jitter_threshold]
    else:
        median_wp99 = 0
        jitter_threshold = 0
        jitter_windows = []
        clean_windows = []

    # --- Clean RTT (excluding jitter windows) ---
    jitter_bids = set(bid for bid, _ in jitter_windows)
    clean_rtt = []
    for t, rtt in zip(t_ms, rtt_us):
        bid = t // bucket_ms
        if bid not in jitter_bids:
            clean_rtt.append(rtt)

    clean_sorted = sorted(clean_rtt) if clean_rtt else []
    clean_p50 = percentile(clean_sorted, 50) if clean_sorted else 0
    clean_p90 = percentile(clean_sorted, 90) if clean_sorted else 0
    clean_p99 = percentile(clean_sorted, 99) if clean_sorted else 0

    # --- Throughput ---
    if duration_s > 0:
        throughput = n / duration_s
    else:
        throughput = 0

    # --- Print report ---
    print(f"\n{'=' * 60}")
    print(f"  {basename}")
    print(f"{'=' * 60}")
    print(f"  Samples:        {n:,}")
    print(f"  Duration:       {duration_s:.1f} s")
    print(f"  Throughput:     {throughput:,.0f} msg/s")
    print()
    print(f"  --- Overall RTT ---")
    print(f"  min:            {format_us(rtt_min)}")
    print(f"  p50 (median):   {format_us(p50)}")
    print(f"  p90:            {format_us(p90)}")
    print(f"  p99:            {format_us(p99)}")
    print(f"  p99.9:          {format_us(p999)}")
    print(f"  max:            {format_us(rtt_max)}")
    print(f"  mean:           {format_us(rtt_mean)}")
    print()
    print(f"  --- Windowed Analysis (10s buckets) ---")
    print(f"  Total windows:  {len(window_p99s)}")
    print(f"  Median of window p99s: {format_us(median_wp99)}")
    print(f"  Jitter threshold (3¡Á): {format_us(jitter_threshold)}")
    print(f"  Jitter windows: {len(jitter_windows)}/{len(window_p99s)}"
          f"  ({100 * len(jitter_windows) / max(len(window_p99s), 1):.1f}%)")

    if jitter_windows:
        print(f"  Worst jitter windows:")
        for bid, wp in sorted(jitter_windows, key=lambda x: -x[1])[:5]:
            t_start = bid * bucket_ms / 1000
            print(f"    t={t_start:.0f}¨C{t_start + 10:.0f}s  p99={format_us(wp)}")

    print()
    print(f"  --- Clean RTT (jitter windows excluded) ---")
    print(f"  Clean samples:  {len(clean_rtt):,} / {n:,}"
          f"  ({100 * len(clean_rtt) / max(n, 1):.1f}%)")
    print(f"  Clean p50:      {format_us(clean_p50)}")
    print(f"  Clean p90:      {format_us(clean_p90)}")
    print(f"  Clean p99:      {format_us(clean_p99)}")

    return {
        "file": basename,
        "n": n,
        "duration_s": duration_s,
        "throughput": throughput,
        "p50": p50,
        "p90": p90,
        "p99": p99,
        "p999": p999,
        "max": rtt_max,
        "mean": rtt_mean,
        "clean_p50": clean_p50,
        "clean_p90": clean_p90,
        "clean_p99": clean_p99,
        "jitter_pct": 100 * len(jitter_windows) / max(len(window_p99s), 1),
        "clean_pct": 100 * len(clean_rtt) / max(n, 1),
    }


def print_summary_table(results):
    """Print a compact comparison table across all files."""
    print(f"\n{'=' * 90}")
    print(f"  SUMMARY TABLE")
    print(f"{'=' * 90}")

    header = (f"  {'File':<20} {'Samples':>10} {'msg/s':>8}"
              f"  {'p50':>8} {'p90':>8} {'p99':>8} {'max':>10}"
              f"  {'clean p99':>10} {'jitter%':>7}")
    print(header)
    print(f"  {'-' * 88}")

    for r in results:
        if r is None:
            continue
        row = (f"  {r['file']:<20} {r['n']:>10,} {r['throughput']:>8,.0f}"
               f"  {r['p50']:>8.0f} {r['p90']:>8.0f} {r['p99']:>8.0f}"
               f" {r['max']:>10.0f}"
               f"  {r['clean_p99']:>10.0f} {r['jitter_pct']:>6.1f}%")
        print(row)

    print(f"\n  (All RTT values in microseconds)")


def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <rtt_csv_file> [rtt_csv_file ...]")
        print(f"Example: {sys.argv[0]} results/rtt_*.csv")
        sys.exit(1)

    files = sys.argv[1:]
    results = []

    for path in sorted(files):
        if not os.path.isfile(path):
            print(f"Warning: {path} not found, skipping")
            continue
        try:
            r = analyze_file(path)
            results.append(r)
        except Exception as e:
            print(f"Error processing {path}: {e}")
            results.append(None)

    if len(results) > 1:
        print_summary_table(results)

    # --- Scaling analysis ---
    valid = [r for r in results if r is not None and r["n"] > 0]
    if len(valid) >= 2:
        first = valid[0]
        last = valid[-1]
        load_ratio = last["throughput"] / max(first["throughput"], 1)
        p99_ratio = last["p99"] / max(first["p99"], 1)

        print(f"\n  --- Scaling Analysis ---")
        print(f"  Load range:  {first['throughput']:,.0f} ¡ú {last['throughput']:,.0f} msg/s"
              f"  ({load_ratio:.1f}¡Á)")
        print(f"  p99 range:   {first['p99']:.0f} ¡ú {last['p99']:.0f} ¦Ìs"
              f"  ({p99_ratio:.1f}¡Á)")

        if p99_ratio < load_ratio:
            print(f"  Verdict:     SUB-LINEAR p99 growth"
                  f" ({p99_ratio:.1f}¡Á p99 over {load_ratio:.1f}¡Á load)"
                  f" ¡ª no architectural degradation detected")
        elif p99_ratio < load_ratio * 1.5:
            print(f"  Verdict:     ROUGHLY LINEAR p99 growth ¡ª acceptable scaling")
        else:
            print(f"  Verdict:     SUPER-LINEAR p99 growth ¡ª potential bottleneck")


if __name__ == "__main__":
    main()