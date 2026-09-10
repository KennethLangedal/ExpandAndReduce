#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# ALENEX Artifact Evaluation Entry Point
# Usage:
#   ./runme.sh          (Runs quick smoke test on data/tiny/)
#   ./runme.sh full     (Runs full benchmark evaluation on all 32 instances)
#   ./runme.sh download (Downloads benchmark dataset from Zenodo)
# -----------------------------------------------------------------------------

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

MODE="${1:-test}"

case "${MODE}" in
    test|quick|demo)
        bash scripts/run_test.sh
        ;;
    full|all|paper)
        bash scripts/run_full.sh
        ;;
    download)
        bash scripts/download_instances.sh
        ;;
    *)
        echo "Usage: $0 [test|full|download]"
        echo "  test     - Run quick smoke test on data/tiny/ (default, < 5s)"
        echo "  full     - Run complete reproduction on all 32 instances"
        echo "  download - Download benchmark dataset from Zenodo"
        exit 1
        ;;
esac
