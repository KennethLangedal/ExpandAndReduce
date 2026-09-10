#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Quick Smoke Test for Expand & Reduce
# Runs ENR on tiny demo instances (< 5 seconds)
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

echo "============================================================"
echo "  Expand & Reduce: Quick Smoke Test"
echo "  (AE reproduction scripts prepared with Gemini 3.7 Flash)"
echo "============================================================"

echo "==> Building ENR..."
make

OUTPUT_DIR="output/test"
mkdir -p "${OUTPUT_DIR}/reduced" "${OUTPUT_DIR}/meta"

echo ""
echo "==> Running ENR on tiny instances in data/tiny/..."
echo "------------------------------------------------------------"
echo "instance,n,m,nk,mk,offset,tred"
echo "------------------------------------------------------------"

for graph_file in data/tiny/*.graph; do
    if [ ! -f "${graph_file}" ]; then
        echo "Error: No graph files found in data/tiny/" >&2
        exit 1
    fi
    base_name="$(basename "${graph_file}")"
    ./ENR "${graph_file}" "${OUTPUT_DIR}/reduced/${base_name}" "${OUTPUT_DIR}/meta/${base_name}.meta"
done

echo "------------------------------------------------------------"
echo "==> Smoke test completed successfully!"
echo "==> Output files generated in ${OUTPUT_DIR}/"
