#!/bin/sh

# Download all astrometry.net index series currently listed on data.astrometry.net.
# WARNING: This can consume very large disk space and take many hours or days.
#
# Usage:
#   ./tools/download-all-astrometry-indexes.sh [output-dir]
#
# Default output-dir: ./conf/astrometry-index

set -eu

BASE_URL="https://data.astrometry.net"
OUT_DIR="${1:-./conf/astrometry-index}"

mkdir -p "$OUT_DIR"

if ! command -v curl >/dev/null 2>&1; then
    echo "ERROR: curl is required"
    exit 1
fi

if ! command -v grep >/dev/null 2>&1; then
    echo "ERROR: grep is required"
    exit 1
fi

echo "Output directory: $OUT_DIR"
echo "Fetching available series list from $BASE_URL ..."

series_list=$(curl -fsSL "$BASE_URL/" | grep -Eo 'href="[0-9]{4}/"' | sed -E 's/href="([0-9]{4})\/"/\1/' | sort -u)

if [ -z "$series_list" ]; then
    echo "ERROR: Could not discover index series from $BASE_URL"
    exit 1
fi

echo "Discovered series:"
printf '%s\n' "$series_list"

echo "Starting download (resume enabled)..."

for series in $series_list; do
    echo "== Series $series =="

    files=$(curl -fsSL "$BASE_URL/$series/" | grep -Eo 'index-[0-9]{4}(-[0-9]{2})?\.fits' | sort -u)

    if [ -z "$files" ]; then
        echo "No index FITS files listed under series $series"
        continue
    fi

    for filename in $files; do
        url="$BASE_URL/$series/$filename"
        out_path="$OUT_DIR/$filename"

        echo "Fetching/resuming: $filename"
        curl -fL -C - "$url" -o "$out_path"
    done

done

echo "All discovered series download attempts completed."
