#!/usr/bin/env bash
set -euo pipefail

# -----------------------------------------------------------------------------
# Download and unpack ALENEX benchmark instances from Zenodo
# DOI: https://doi.org/10.5281/zenodo.22661369
# -----------------------------------------------------------------------------

ZENODO_URL="${ZENODO_URL:-https://zenodo.org/records/22661369/files/ExpandAndReduce-Data.tar.gz?download=1}"
EXPECTED_MD5="33683cdff3ef76508f15faf05ff838af"
ARCHIVE_NAME="ExpandAndReduce-Data.tar.gz"

# Determine project root directory (one level above this script)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
TARGET_DIR="${ROOT_DIR}/data/full"
ARCHIVE_PATH="${ROOT_DIR}/${ARCHIVE_NAME}"

echo "==> Creating target directory: data/full/"
mkdir -p "${TARGET_DIR}"

echo "==> Downloading instances from Zenodo (${ZENODO_URL})..."
if command -v curl &> /dev/null; then
    curl -L --progress-bar -o "${ARCHIVE_PATH}" "${ZENODO_URL}"
elif command -v wget &> /dev/null; then
    wget --progress=bar:force -O "${ARCHIVE_PATH}" "${ZENODO_URL}"
else
    echo "Error: Neither curl nor wget found on system." >&2
    exit 1
fi

echo "==> Verifying MD5 checksum..."
if command -v md5sum &> /dev/null; then
    echo "${EXPECTED_MD5}  ${ARCHIVE_PATH}" | md5sum -c -
elif command -v md5 &> /dev/null; then
    ACTUAL_MD5=$(md5 -q "${ARCHIVE_PATH}")
    if [ "${ACTUAL_MD5}" != "${EXPECTED_MD5}" ]; then
        echo "Error: MD5 mismatch! Expected ${EXPECTED_MD5}, got ${ACTUAL_MD5}" >&2
        exit 1
    fi
    echo "${ARCHIVE_NAME}: OK"
else
    echo "Warning: md5sum tool not found. Skipping checksum verification."
fi

echo "==> Extracting instances into data/full/..."
tar -xzf "${ARCHIVE_PATH}" --strip-components=1 -C "${TARGET_DIR}"

echo "==> Cleaning up archive file..."
rm -f "${ARCHIVE_PATH}"

echo "==> Successfully downloaded and unpacked 32 benchmark instances into data/full/"
