#!/bin/sh

# Download precomputed astrometry.net index files (quad invariants).
# Example:
#   ./download-astrometry-indexes.sh 4200 00 11 ../conf/astrometry-index
# This downloads index-4200-00.fits ... index-4200-11.fits

set -eu

BASE_URL="https://data.astrometry.net"

if [ "$#" -lt 3 ] || [ "$#" -gt 4 ]; then
    echo "Usage: $0 <series> <start> <end> [output-dir]"
    echo "  series: index family, e.g. 4100, 4200, 5200"
    echo "  start/end: two-digit file suffix range, e.g. 00 11"
    echo "  output-dir default: ./astrometry-index"
    exit 1
fi

SERIES="$1"
START="$2"
END="$3"
OUT_DIR="${4:-./astrometry-index}"

mkdir -p "$OUT_DIR"

echo "Downloading astrometry indexes into: $OUT_DIR"

i="$START"
while [ "$i" -le "$END" ]; do
    suffix=$(printf "%02d" "$i")
    filename="index-${SERIES}-${suffix}.fits"
    url="${BASE_URL}/${SERIES}/${filename}"
    out_path="${OUT_DIR}/${filename}"

    echo "Fetching/resuming: $filename"
    curl -fL -C - "$url" -o "$out_path"

    i=$((i + 1))
done

echo "Done."