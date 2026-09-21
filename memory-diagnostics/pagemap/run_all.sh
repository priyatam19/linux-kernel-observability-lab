#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for mode in 4KB 2MB 2GB; do
    ./memalloc "$mode" "${1:-2048}"
    python3 plot_cdf.py "latency_log_${mode}.csv" "$mode"
done
