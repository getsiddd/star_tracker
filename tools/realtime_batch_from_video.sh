#!/usr/bin/env bash
set -euo pipefail

# Real-time-ish test runner for LOST blind-solve-batch.
# Supports either a local video file or a live webcam capture on macOS (avfoundation).

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT_DIR/bin/lost"

SOURCE="video"
VIDEO_PATH=""
VIDEO_URL=""
CAMERA_INDEX="0"
FPS="1"
DURATION_SEC="30"
MAX_FRAMES=""
INDEX_DIR="$ROOT_DIR/conf/astrometry-index"
OUT_DIR="$ROOT_DIR/logs/realtime-run-$(date +%Y%m%d_%H%M%S)"
SCALE_LOW="10"
SCALE_HIGH="400"
DOWNSAMPLE="2"
TIMEOUT_SEC="60"
JOBS="2"
TARGET_WIDTH="720"
FAST_TIMEOUT_SEC="20"
FALLBACK_TIMEOUT_SEC="120"
FALLBACK_ON_FAIL="on"
SOLVE_EVERY_NTH="1"
FAST_TIMEOUT_SET="0"
FALLBACK_TIMEOUT_SET="0"

usage() {
  cat <<EOF
Usage: $(basename "$0") [options]

Options:
  --source <video|webcam>       Input source type (default: video)
  --video-path <path>           Local video file path
  --video-url <url>             Download this video URL before processing
  --camera-index <n>            Webcam index for avfoundation (default: 0)
  --fps <n>                     Frame sampling FPS (default: 1)
  --duration-sec <n>            Webcam capture duration in seconds (default: 30)
  --max-frames <n>              Stop after N extracted frames
  --index-dir <dir>             Astrometry index directory
  --output-dir <dir>            Output directory
  --scale-low <v>               Lower plate scale (arcsec/pixel)
  --scale-high <v>              Upper plate scale (arcsec/pixel)
  --downsample <n>              Solve downsample
  --timeout <n>                 Per-image solver timeout
  --fast-timeout <n>            Fast pass timeout (default: 20)
  --fallback-timeout <n>        Fallback timeout for failed frames (default: 120)
  --fallback-on-fail <on|off>   Retry failed frames with fallback pass (default: on)
  --solve-every-nth <n>         Keep every Nth frame before solve (default: 1)
  --jobs <n>                    Batch workers
  --target-width <px>           Global preprocessing target width (default: 720)
  --help                        Show this help

Examples:
  $(basename "$0") --source video --video-path /path/to/night.mp4 --fps 1
  $(basename "$0") --source webcam --camera-index 0 --fps 1 --duration-sec 60
  $(basename "$0") --video-url https://raw.githubusercontent.com/opencv/opencv/master/samples/data/vtest.avi --fps 1
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --source) SOURCE="$2"; shift 2 ;;
    --video-path) VIDEO_PATH="$2"; shift 2 ;;
    --video-url) VIDEO_URL="$2"; shift 2 ;;
    --camera-index) CAMERA_INDEX="$2"; shift 2 ;;
    --fps) FPS="$2"; shift 2 ;;
    --duration-sec) DURATION_SEC="$2"; shift 2 ;;
    --max-frames) MAX_FRAMES="$2"; shift 2 ;;
    --index-dir) INDEX_DIR="$2"; shift 2 ;;
    --output-dir) OUT_DIR="$2"; shift 2 ;;
    --scale-low) SCALE_LOW="$2"; shift 2 ;;
    --scale-high) SCALE_HIGH="$2"; shift 2 ;;
    --downsample) DOWNSAMPLE="$2"; shift 2 ;;
    --timeout) TIMEOUT_SEC="$2"; shift 2 ;;
    --fast-timeout) FAST_TIMEOUT_SEC="$2"; FAST_TIMEOUT_SET="1"; shift 2 ;;
    --fallback-timeout) FALLBACK_TIMEOUT_SEC="$2"; FALLBACK_TIMEOUT_SET="1"; shift 2 ;;
    --fallback-on-fail) FALLBACK_ON_FAIL="$2"; shift 2 ;;
    --solve-every-nth) SOLVE_EVERY_NTH="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --target-width) TARGET_WIDTH="$2"; shift 2 ;;
    --help) usage; exit 0 ;;
    *) echo "Unknown option: $1"; usage; exit 1 ;;
  esac
done

# Backward compatibility: if only --timeout is passed, apply it to both passes.
if [[ "$FAST_TIMEOUT_SET" == "0" ]]; then
  FAST_TIMEOUT_SEC="$TIMEOUT_SEC"
fi
if [[ "$FALLBACK_TIMEOUT_SET" == "0" ]]; then
  FALLBACK_TIMEOUT_SEC="$TIMEOUT_SEC"
fi

if [[ ! -x "$BIN" ]]; then
  echo "ERROR: missing binary: $BIN"
  echo "Build first: cmake --build . -j4"
  exit 1
fi

if ! [[ "$SOLVE_EVERY_NTH" =~ ^[0-9]+$ ]] || [[ "$SOLVE_EVERY_NTH" -lt 1 ]]; then
  echo "ERROR: --solve-every-nth must be an integer >= 1"
  exit 1
fi

if [[ "$FALLBACK_ON_FAIL" != "on" && "$FALLBACK_ON_FAIL" != "off" ]]; then
  echo "ERROR: --fallback-on-fail must be on or off"
  exit 1
fi

if ! command -v ffmpeg >/dev/null 2>&1; then
  echo "ERROR: ffmpeg is required. Install with: brew install ffmpeg"
  exit 1
fi

mkdir -p "$OUT_DIR/frames"
FRAMES_DIR="$OUT_DIR/frames"

if [[ -n "$VIDEO_URL" ]]; then
  VIDEO_PATH="$OUT_DIR/input_video_$(basename "$VIDEO_URL")"
  echo "Downloading video from URL..."
  curl -fL "$VIDEO_URL" -o "$VIDEO_PATH"
fi

if [[ "$SOURCE" == "video" ]]; then
  if [[ -z "$VIDEO_PATH" || ! -f "$VIDEO_PATH" ]]; then
    echo "ERROR: --video-path must point to an existing file (or use --video-url)"
    exit 1
  fi
  echo "Extracting frames from video: $VIDEO_PATH"
  ffmpeg -hide_banner -loglevel error -y -i "$VIDEO_PATH" -vf "fps=${FPS}" "$FRAMES_DIR/frame_%06d.png"
elif [[ "$SOURCE" == "webcam" ]]; then
  # macOS avfoundation format: "<video_index>:<audio_index>". Use "none" for audio.
  echo "Capturing webcam frames from camera index ${CAMERA_INDEX}"
  ffmpeg -hide_banner -loglevel error -y \
    -f avfoundation -framerate "$FPS" -i "${CAMERA_INDEX}:none" \
    -t "$DURATION_SEC" \
    "$FRAMES_DIR/frame_%06d.png"
else
  echo "ERROR: --source must be video or webcam"
  exit 1
fi

if [[ -n "$MAX_FRAMES" ]]; then
  tmp_dir="$OUT_DIR/frames_limited"
  mkdir -p "$tmp_dir"
  shopt -s nullglob
  frames=("$FRAMES_DIR"/*.png)
  shopt -u nullglob

  for frame in "${frames[@]:0:${MAX_FRAMES}}"; do
    cp "$frame" "$tmp_dir/"
  done
  FRAMES_DIR="$tmp_dir"
fi

if [[ "$SOLVE_EVERY_NTH" -gt 1 ]]; then
  tmp_stride_dir="$OUT_DIR/frames_stride"
  mkdir -p "$tmp_stride_dir"
  shopt -s nullglob
  frames=($(ls "$FRAMES_DIR"/*.png 2>/dev/null | sort))
  shopt -u nullglob

  kept=0
  idx=0
  for frame in "${frames[@]}"; do
    if (( idx % SOLVE_EVERY_NTH == 0 )); then
      cp "$frame" "$tmp_stride_dir/"
      kept=$((kept + 1))
    fi
    idx=$((idx + 1))
  done

  if [[ "$kept" -gt 0 ]]; then
    FRAMES_DIR="$tmp_stride_dir"
  fi
fi

frame_count=$(find "$FRAMES_DIR" -maxdepth 1 -type f -name '*.png' | wc -l | tr -d ' ')
if [[ "$frame_count" == "0" ]]; then
  echo "ERROR: no frames extracted"
  exit 1
fi

FAST_OUT_DIR="$OUT_DIR/solve_fast"
echo "Running fast blind-solve-batch on ${frame_count} frames..."
"$BIN" blind-solve-batch \
  --input-dir "$FRAMES_DIR" \
  --index-dir "$INDEX_DIR" \
  --output-dir "$FAST_OUT_DIR" \
  --scale-low "$SCALE_LOW" \
  --scale-high "$SCALE_HIGH" \
  --downsample "$DOWNSAMPLE" \
  --timeout "$FAST_TIMEOUT_SEC" \
  --jobs "$JOBS" \
  --preprocess on \
  --preprocess-mode global \
  --target-width "$TARGET_WIDTH" \
  --max-dim 2200 \
  --overwrite

MERGED_SUMMARY="$OUT_DIR/solve/summary_merged.tsv"
mkdir -p "$OUT_DIR/solve"
cp "$FAST_OUT_DIR/summary.tsv" "$MERGED_SUMMARY"

if [[ "$FALLBACK_ON_FAIL" == "on" ]]; then
  failed_dir="$OUT_DIR/failed_frames"
  mkdir -p "$failed_dir"

  awk -F '\t' 'NR>1 && $2 != "solved" {print $1}' "$FAST_OUT_DIR/summary.tsv" | while IFS= read -r img; do
    [[ -z "$img" ]] && continue
    if [[ -f "$FRAMES_DIR/$img" ]]; then
      cp "$FRAMES_DIR/$img" "$failed_dir/$img"
    fi
  done

  failed_count=$(find "$failed_dir" -maxdepth 1 -type f -name '*.png' | wc -l | tr -d ' ')
  if [[ "$failed_count" -gt 0 ]]; then
    echo "Fast pass failed on ${failed_count} frames. Running fallback pass..."
    FALLBACK_OUT_DIR="$OUT_DIR/solve_fallback"
    "$BIN" blind-solve-batch \
      --input-dir "$failed_dir" \
      --index-dir "$INDEX_DIR" \
      --output-dir "$FALLBACK_OUT_DIR" \
      --scale-low "$SCALE_LOW" \
      --scale-high "$SCALE_HIGH" \
      --downsample "$DOWNSAMPLE" \
      --timeout "$FALLBACK_TIMEOUT_SEC" \
      --jobs "$JOBS" \
      --preprocess on \
      --preprocess-mode global \
      --target-width "$TARGET_WIDTH" \
      --max-dim 2200 \
      --overwrite

    awk -F '\t' 'NR==FNR{if(FNR>1) fb[$1]=$0; next} FNR==1{print; next} { if ($1 in fb) print fb[$1]; else print }' \
      "$FALLBACK_OUT_DIR/summary.tsv" "$FAST_OUT_DIR/summary.tsv" > "$MERGED_SUMMARY"
  fi
fi

echo
echo "Done. Key outputs:"
echo "  Frames:       $FRAMES_DIR"
echo "  Fast logs:    $FAST_OUT_DIR"
echo "  Merged TSV:   $MERGED_SUMMARY"
