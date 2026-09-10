#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Full Paper Reproduction for Expand & Reduce
# Runs ENR on all 32 benchmark instances from results.csv
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${ROOT_DIR}"

echo "============================================================"
echo "  Expand & Reduce: Full Paper Reproduction"
echo "  (AE reproduction scripts prepared with Gemini 3.7 Flash)"
echo "============================================================"

# Check if benchmark dataset exists
if [ ! -d "data/full" ] || [ -z "$(ls -A data/full/*.graph 2>/dev/null)" ]; then
    echo "==> Benchmark dataset not found in data/full/."
    echo "==> Automatically downloading instances from Zenodo..."
    bash "${SCRIPT_DIR}/download_instances.sh"
fi

echo "==> Building ENR..."
make

OUTPUT_CSV="results_reproduced.csv"
OUTPUT_DIR="output/full"
mkdir -p "${OUTPUT_DIR}/reduced" "${OUTPUT_DIR}/meta"

echo "instance,n,m,nk,mk,offset,tred" > "${OUTPUT_CSV}"

echo ""
echo "==> Running Expand & Reduce on all 32 benchmark instances..."
echo "==> Results will be saved to ${OUTPUT_CSV}"
echo "Note: Full evaluation may take several hours depending on hardware."
echo "------------------------------------------------------------"

# Read instance list from results.csv (skipping header)
tail -n +2 results.csv | cut -d',' -f1 | while IFS= read -r inst_name; do
    inst_path="data/full/${inst_name}"
    if [ ! -f "${inst_path}" ]; then
        echo "Warning: Instance ${inst_path} not found, skipping..." >&2
        continue
    fi
    echo "[$(date '+%H:%M:%S')] Processing: ${inst_name}"
    
    # Run ENR and capture output to CSV and stdout
    res=$(./ENR "${inst_path}" "${OUTPUT_DIR}/reduced/${inst_name}" "${OUTPUT_DIR}/meta/${inst_name}.meta")
    echo "${res}"
    echo "${res}" >> "${OUTPUT_CSV}"
done

echo "------------------------------------------------------------"
echo "==> Full benchmark evaluation complete!"
echo "==> Results recorded in: ${OUTPUT_CSV}"
echo "==> Reduced graphs saved in: ${OUTPUT_DIR}/reduced/"
echo "==> Meta files saved in: ${OUTPUT_DIR}/meta/"
