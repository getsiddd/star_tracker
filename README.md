# LOST Star Tracker: Build and Run Guide

This project includes a blind star-field solver wrapper around `astrometry.net`.

## 1. Prerequisites

On macOS (Homebrew):

```bash
brew install cmake boost astrometry-net exiftool
```

Required external tools used by the solver:
- `solve-field` (from astrometry.net)
- `wcsinfo` (from astrometry.net)
- `sips` (built into macOS)

## 2. Build

From the project root:

```bash
cd /Users/stdaux-001/StdAux/Projects/ACDS/star_tracker
cmake -S . -B .
cmake --build . -j4
```

Binary output:

```bash
./bin/lost
```

## 3. Download Astrometry Index Files

Create the index directory (if missing):

```bash
mkdir -p conf/astrometry-index
```

### Recommended minimum set for mixed DSLR + narrower fields

```bash
# Tycho-2 wide-field series (good for wide DSLR frames)
for i in 4119 4118 4117 4116 4115 4114 4113 4112 4111 4110 4109; do
  curl -fL -C - "https://data.astrometry.net/4100/index-${i}.fits" \
    -o "conf/astrometry-index/index-${i}.fits"
done

# 2MASS medium/smaller-field series
for i in 4210 4211 4212 4213 4214 4215 4216 4217 4218 4219; do
  curl -fL -C - "https://data.astrometry.net/4200/index-${i}.fits" \
    -o "conf/astrometry-index/index-${i}.fits"
done
```

### Scripted download options

```bash
# Multipart/suffix-based downloader
./tools/download-astrometry-indexes.sh 4200 00 11 conf/astrometry-index

# Crawl and download all published index families/files (large disk/time)
./tools/download-all-astrometry-indexes.sh conf/astrometry-index
```

## 4. Quarantine Corrupt Index Files

If astrometry reports errors like `Kdtree header was not found`, move the bad file out of the main index directory:

```bash
mkdir -p conf/astrometry-index/quarantine
mv conf/astrometry-index/index-4200-00.fits conf/astrometry-index/quarantine/
```

Note: the current solver config builder scans only the top-level `conf/astrometry-index`, so quarantined files are ignored.

## 5. Single Image Solve

```bash
./bin/lost blind-solve \
  --image sample/_AST0931.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-single \
  --scale-low 10 --scale-high 400 \
  --downsample 2 \
  --timeout 300 \
  --preprocess on \
  --preprocess-mode global \
  --max-dim 2200 \
  --overwrite
```

Key flags:
- `--preprocess on|off` (default on)
- `--preprocess-mode global|tiles` (default global)
- `--max-dim <px>`: global longest-side limit
- `--tile-width`, `--tile-height`, `--tile-overlap`: tile mode controls
- `--timeout <sec>`: per-image CPU limit for solve-field

## 6. Batch Solve

```bash
./bin/lost blind-solve-batch \
  --input-dir sample \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-batch \
  --scale-low 10 --scale-high 400 \
  --downsample 2 \
  --timeout 300 \
  --jobs 4 \
  --preprocess on \
  --preprocess-mode global \
  --max-dim 2200 \
  --overwrite
```

Batch output includes:
- per-image lines: `batch_done success|failure ...`
- summary line: `batch_summary total=... solved=... failed=...`
- auto-generated machine table: `<output-dir>/summary.tsv`
- auto-generated human table: `<output-dir>/summary_pretty.tsv`

Precision policy:
- `summary.tsv` is machine-precision first (for fusion and post-processing).
- `summary_pretty.tsv` is rounded for easier human reading.

`summary.tsv` columns include:
- `ra_deg`, `dec_deg` (Center RA/Dec)
- `ra_hms`, `dec_dms` (Center in HMS/DMS)
- `size_w_arcmin`, `size_h_arcmin` (field size)
- `radius_deg` (half-diagonal field radius)
- `pixel_scale_arcsec_per_pix`

## 7. Real-Time / Throughput Tuning

Use these in order:

1. Keep `--preprocess on --preprocess-mode global --max-dim 2200`.
2. Use `--jobs` near physical core count for throughput runs.
3. Keep index set targeted to your lens/FOV; too many irrelevant indexes increases search time.
4. Quarantine any invalid index files immediately.
5. For very large images, try tile mode:

```bash
--preprocess on --preprocess-mode tiles --tile-width 1800 --tile-height 1800 --tile-overlap 300
```

## 8. Why Some Images Take Too Long

Most long runs are caused by one or more of:
- Missing index coverage for the image field of view
- Corrupt index file still being loaded
- High source counts with no matching quads (solver consumes full timeout)
- Overly broad scale range and/or excessive timeout

## 9. Quick Verification Commands

```bash
# List indexes currently available
ls conf/astrometry-index | sort

# Confirm blind-solve help/flags
./bin/lost blind-solve --help
./bin/lost blind-solve-batch --help

# Check if a solve process is active
ps -ef | grep "bin/lost blind-solve" | grep -v grep
```

## 10. Common Paths

- Binary: `bin/lost`
- Indexes: `conf/astrometry-index`
- Quarantine: `conf/astrometry-index/quarantine`
- Logs/artifacts: `logs/`

## 11. Real-Time Testing (Webcam or Video)

Install additional tools for real-time test scripts:

```bash
brew install ffmpeg
pip install pyyaml numpy
```

Use the built-in runner script:

```bash
chmod +x tools/realtime_batch_from_video.sh
```

### A) Test with a local video file

```bash
./tools/realtime_batch_from_video.sh \
  --source video \
  --video-path /absolute/path/to/night_sky.mp4 \
  --fps 1 \
  --jobs 4
```

### B) Test with a webcam (macOS avfoundation)

```bash
./tools/realtime_batch_from_video.sh \
  --source webcam \
  --camera-index 0 \
  --fps 1 \
  --duration-sec 60 \
  --jobs 2
```

### C) Test with an internet video directly

```bash
./tools/realtime_batch_from_video.sh \
  --video-url https://raw.githubusercontent.com/opencv/opencv/master/samples/data/vtest.avi \
  --fps 1 \
  --jobs 2
```

This script extracts frames and runs `blind-solve-batch` on them. The run still auto-generates:

- `<output-dir>/solve/summary.tsv`
- `<output-dir>/solve/summary_pretty.tsv`

## 12. Multi-Camera Body Fusion (`camera.yml`)

### 3D GUI editor for `camera.yml`

You can create/edit `camera.yml` visually with a 3D satellite model:

- Tool path: `tools/camera-yml-gui/index.html`
- It lets you place star trackers on the satellite body and set orientation.
- It exports a ready-to-use `camera.yml`.

Run locally:

```bash
cd tools/camera-yml-gui
python3 -m http.server 8080
```

Then open:

- http://localhost:8080

GUI workflow:

1. Click `Add Tracker` for each camera.
2. Set `id`, `device_id`, and `summary_tsv`.
3. Set `position_body` (x, y, z) to place the tracker on the satellite.
4. Set orientation with yaw/pitch/roll (deg).
5. Click `Generate YAML` or `Download camera.yml`.

Frame convention (important):

- All exported coordinates are with respect to the satellite body frame.
- Mouse drag rotates the camera view only (not the underlying body frame data).
- `position_body`, `boresight_body`, `up_body`, and `yaw_pitch_roll_deg` are all body-frame referenced.

Exported camera fields from the GUI:

- `position_body`: tracker mounting position in body frame.
- `yaw_pitch_roll_deg`: input orientation values.
- `boresight_body`: derived optical axis unit vector.
- `up_body`: derived image +Y unit vector.

Notes:

- The fusion script currently uses `boresight_body` and `summary_tsv` directly.
- `position_body` and `yaw_pitch_roll_deg` are still useful for hardware layout, validation, and future extensions.

Template file:

- `conf/camera.yml`

It defines multiple cameras, each with:

- `id`, `device_id`
- `boresight_body` (camera optical axis in body frame)
- `up_body` (camera +Y direction in body frame)
- `summary_tsv` (per-camera tracker output file)

### `camera.yml` field explanation

Top-level:

- `frame.reference`: coordinate reference used by the file (set to `satellite_body`)
- `cameras`: list of camera definitions to include in fusion
- `fusion.min_cameras`: minimum solved cameras required to run fusion
- `fusion.weighting`: weighting mode (`inverse_pixel_scale` is currently supported)

Per camera entry:

- `id`: unique camera name used in logs and fusion output
- `device_id`: capture device ID (used by your capture pipeline; informational for fusion script)
- `boresight_body`: 3D unit vector `[x, y, z]` of camera optical axis in spacecraft body frame
- `up_body`: 3D unit vector `[x, y, z]` of camera image +Y direction in spacecraft body frame
- `summary_tsv`: path to that camera run's `summary.tsv` generated by `blind-solve-batch`

Coordinate/quality rules:

- `boresight_body` and `up_body` should be normalized or close to unit length.
- `up_body` should not be parallel to `boresight_body`.
- Use one consistent body-frame convention across all cameras (for example, +X forward, +Y right, +Z down).
- For robust fusion, include at least 2 cameras with significantly different boresight directions.

Example:

```yaml
cameras:
  - id: cam0
    device_id: /dev/video0
    boresight_body: [1.0, 0.0, 0.0]
    up_body: [0.0, 0.0, 1.0]
    summary_tsv: logs/cam0_run/summary.tsv

  - id: cam1
    device_id: /dev/video1
    boresight_body: [0.0, 1.0, 0.0]
    up_body: [0.0, 0.0, 1.0]
    summary_tsv: logs/cam1_run/summary.tsv

fusion:
  min_cameras: 2
  weighting: inverse_pixel_scale
```

### End-to-end workflow using `camera.yml`

1. Run each camera/feed independently and write each output to its own folder:

```bash
./tools/realtime_batch_from_video.sh --source webcam --camera-index 0 --output-dir logs/cam0_run
./tools/realtime_batch_from_video.sh --source webcam --camera-index 1 --output-dir logs/cam1_run
```

2. Ensure each run produced:

- `logs/cam0_run/solve/summary.tsv`
- `logs/cam1_run/solve/summary.tsv`

3. Point `summary_tsv` in `conf/camera.yml` to those files.

4. Run fusion:

```bash
./tools/fuse_multi_star_tracker.py \
  --camera-config conf/camera.yml \
  --output logs/fusion_result.txt
```

5. Inspect `logs/fusion_result.txt` for:

- used camera count
- fused `R_b_to_i` rotation matrix
- fused RA/Dec estimate

Fuse multiple star tracker outputs into one body attitude estimate:

```bash
chmod +x tools/fuse_multi_star_tracker.py
./tools/fuse_multi_star_tracker.py \
  --camera-config conf/camera.yml \
  --output logs/fusion_result.txt
```

Fusion method used in the script:

1. Read latest solved row from each camera's `summary.tsv`
2. Convert `(ra_deg, dec_deg)` to inertial unit vectors
3. Pair them with `boresight_body` vectors from `camera.yml`
4. Solve Wahba's problem via SVD (weighted)
5. Report fused body-to-inertial rotation matrix and fused boresight RA/Dec

## 13. Research-Backed Direction for Multi-Star-Tracker Accuracy

Current best-practice direction (used by many ADCS pipelines):

1. Use per-sensor quality weights (pixel scale, centroid residuals, matched stars).
2. Fuse vectors with Wahba/Davenport/QUEST or SVD-based solvers.
3. Feed fused attitude into an EKF/MEKF with gyro propagation.
4. Use robust outlier rejection (RANSAC, M-estimators, innovation gating).
5. Keep camera extrinsics calibrated (`boresight_body`, `up_body`), and re-calibrate in-orbit if needed.

Internet sources reviewed while preparing this flow:

- Wahba's problem overview and references (Wahba 1965; Shuster/Oh 1981; Markley/Mortari 2000):
  - https://en.wikipedia.org/wiki/Wahba%27s_problem
- ArXiv broad query on "star tracker attitude fusion" (few direct hits under that exact phrasing):
  - https://arxiv.org/search/?query=star+tracker+attitude+fusion&searchtype=all

Practical note: the strongest improvements for your project right now are usually from better index coverage and robust multi-camera fusion weighting, not just increasing compute.
