# LOST Star Tracker: Build and Run Guide

This project includes a blind star-field solver wrapper around `astrometry.net`.

## Documentation

Project documentation is now organized under `docs/`:

- `docs/README.md`
- `docs/quickstart/ecef-quickstart.md`
- `docs/guides/blind-solve.md`
- `docs/guides/camera-yml-workflows.md`
- `docs/guides/ecef-attitude-guide.md`
- `docs/reference/cli-blind-solve.md`
- `docs/reference/camera-yml-schema.md`
- `docs/operations/performance-troubleshooting.md`

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
  --timeout 400 \
  --preprocess on \
  --preprocess-mode global \
  --max-dim 2200 \
  --ecef on \
  --3d-viz on \
  --viz-output logs/blind-solve-single/ecef_3d.png \
  --ecef-output logs/blind-solve-single/ecef_attitude.json \
  --ecef-console on \
  --overwrite
```

Key flags:
- `--preprocess on|off` (default on)
- `--preprocess-mode global|tiles` (default global)
- `--max-dim <px>`: global longest-side limit
- `--target-width <px>`: global mode target width (height kept proportional)
- `--tile-width`, `--tile-height`, `--tile-overlap`: tile mode controls
- `--timeout <sec>`: per-image CPU limit for solve-field
- `--ecef on|off` (default on): enable ECEF transform and output
- `--3d-viz on|off` (default on): enable 3D Earth/satellite visualization generation
- `--viz-output <file>`: output PNG path for 3D plot
- `--ecef-output <file>`: output JSON path for rich ECEF metadata
- `--ecef-console on|off` (default on): print ECEF formatted block to console

Notes on image resizing/performance:
- If `--target-width` is set (for example `720`) and input width is larger, image is resized to that width before solve.
- This is often faster for very large DSLR/stacked images while keeping aspect ratio.
- If `--target-width` is not set or image width is already smaller, normal `--max-dim` behavior applies.

### Run From camera.yml (Modes Kept Separate)

`conf/camera.yml` now keeps solve workflows separated under `solve_modes`:
- `solve_modes.single`: single-image per camera
- `solve_modes.folder`: whole-folder batch per camera
- `solve_modes.live`: live webcam per camera

Use the same runner with explicit mode selection:

```bash
# Single-image mode
python3 tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/single

# Whole-folder mode
python3 tools/blind_solve_from_camera_yml.py \
  --mode folder \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/folder

# Live webcam mode
python3 tools/blind_solve_from_camera_yml.py \
  --mode live \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/live
```

You can also run camera.yml workflows directly from LOST binary:

```bash
# Single/folder/live from camera.yml via LOST
./bin/lost camera-yml --mode single --camera-config conf/camera.yml
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml
./bin/lost camera-yml --mode live --camera-config conf/camera.yml

# Run fusion after a multi-camera mode run
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml --run-fusion \
  --fusion-output logs/fusion_result.txt
```

Each mode has its own `defaults` block and camera list; per-camera keys override defaults.

Outputs:
- Per-camera output directory: `<results-dir>/<camera_id>/`
- Per-camera logs: `<results-dir>/<camera_id>/run.log`
- Combined timing table: `<results-dir>/timings.tsv`
- In `single` mode: per-camera ECEF JSON at `<results-dir>/<camera_id>/ecef_attitude.json`

Fusion behavior for multi-camera runs:
- Use `--run-fusion` with `./bin/lost camera-yml ...` to trigger fusion after solve.
- Fusion uses `cameras` + `summary_tsv` entries in `camera.yml` and writes the requested fusion output.

### ECEF JSON Output Schema

When `--ecef-output` is set, the generated JSON now includes solve metadata, time, geodetic coordinates, and full attitude information with explicit keys.

Example structure:

```json
{
  "timestamp_utc": "2026-05-17T10:28:48Z",
  "time": {
    "julian_date": 2461177.936667,
    "gmst_rad": 3.283529,
    "gmst_deg": 188.131231
  },
  "blind_solve": {
    "success": true,
    "ra_deg": 18.049400,
    "dec_deg": 63.460600,
    "ra_hms": "01:12:11.850",
    "dec_dms": "+63:27:38.182",
    "rotation_deg": -172.429000,
    "plate_scale_arcsec_per_pix": 93.744200,
    "field_width_arcmin": 1527.600000,
    "field_height_arcmin": 1032.600000,
    "field_radius_deg": 15.365500,
    "wcs_file": "logs/blind-solve-single/attempt_1/....wcs"
  },
  "ecef": {
    "quaternion": {
      "real": -0.527366,
      "i": 0.068093,
      "j": -0.846093,
      "k": -0.007701
    },
    "euler_angles_rad": {
      "ra": 3.419804,
      "dec": 1.100322,
      "roll": 3.331668
    },
    "axis_directions": {
      "x": [-0.433181, -0.107104, -0.893451],
      "y": [-0.123349, 0.989293, -0.058789],
      "z": [0.891353, 0.084852, -0.442336]
    }
  },
  "position_info": {
    "ecef_km": {
      "x": 6778.137000,
      "y": 0.000000,
      "z": 0.000000
    },
    "distance_from_earth_center_km": 6778.137000
  },
  "geodetic": {
    "latitude_deg": 0.000000,
    "longitude_deg": 0.000000,
    "altitude_km": 400.000000
  }
}
```

Compatibility keys (`position`, `quaternion`, `euler_angles`, `x_axis`, `y_axis`, `z_axis`) remain present for existing tooling.

### Benchmark All Sample Images (Per-Image Time)

The command below runs `blind-solve` on each image in `sample/`, records elapsed seconds, solve status, RA/Dec, and output JSON path.

```bash
mkdir -p logs/benchmark-sample
printf "image\tstatus\telapsed_sec\tra_deg\tdec_deg\tecef_json\n" > logs/benchmark-sample/timings.tsv

for img in sample/*.{jpg,JPG,jpeg,JPEG,png,PNG,tif,tiff,TIF,TIFF}; do
  [ -f "$img" ] || continue
  name="$(basename "$img")"
  outdir="logs/benchmark-sample/${name%.*}"
  mkdir -p "$outdir"

  start=$(date +%s)
  out=$(./bin/lost blind-solve \
    --image "$img" \
    --index-dir conf/astrometry-index \
    --output-dir "$outdir" \
    --scale-low 10 --scale-high 400 \
    --downsample 2 \
    --timeout 400 \
    --preprocess on \
    --preprocess-mode global \
    --max-dim 2200 \
    --ecef on \
    --3d-viz off \
    --ecef-output "$outdir/ecef_attitude.json" \
    --overwrite 2>&1)
  end=$(date +%s)

  elapsed=$((end - start))
  status=$(printf "%s\n" "$out" | awk '/blind_solve_success/{print $2}' | tail -1)
  ra=$(printf "%s\n" "$out" | awk '/blind_solve_ra_deg/{print $2}' | tail -1)
  dec=$(printf "%s\n" "$out" | awk '/blind_solve_dec_deg/{print $2}' | tail -1)

  printf "%s\t%s\t%s\t%s\t%s\t%s\n" \
    "$name" "${status:-0}" "$elapsed" "${ra:-NA}" "${dec:-NA}" "$outdir/ecef_attitude.json" \
    >> logs/benchmark-sample/timings.tsv
done
```

## 6. Batch Solve

```bash
./bin/lost blind-solve-batch \
  --input-dir sample \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-batch \
  --scale-low 10 --scale-high 400 \
  --downsample 2 \
  --timeout 400 \
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

## Quickstart: Use Exactly This

### 1. Sync Binary
Ensure the latest build binary is synced to `./bin/lost`:
```bash
cp ./build/bin/lost ./bin/lost
```

### 2. Direct Per-Image Solve
```bash
./bin/lost blind-solve \
  --image sample/_AST0931.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-single \
  --scale-low 10 --scale-high 400 \
  --downsample 2 \
  --timeout 400 \
  --preprocess on \
  --preprocess-mode global \
  --target-width 720 \
  --max-dim 2200 \
  --ecef on \
  --3d-viz on \
  --viz-output logs/blind-solve-single/ecef_3d.png \
  --ecef-output logs/blind-solve-single/ecef_attitude.json \
  --ecef-console on \
  --overwrite
```

### 3. camera.yml Modes
#### Single Mode
```bash
./bin/lost camera-yml --mode single --camera-config conf/camera.yml
```

#### Folder Mode
```bash
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml
```

#### Live Mode
```bash
./bin/lost camera-yml --mode live --camera-config conf/camera.yml
```

#### Folder Mode + Fusion
```bash
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml --run-fusion \
  --fusion-output logs/fusion_result.txt
```
