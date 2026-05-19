# LOST Star Tracker

LOST is a star-tracker and blind-solve toolkit built around `astrometry.net`, ECEF attitude export, and camera-driven workflows.

The current command surface is defined in `include/options.hpp`. The most important operational commands are:

- `./bin/lost blind-solve`
- `./bin/lost blind-solve-batch`
- `./bin/lost camera-yml`

## Documentation

Primary docs live under `docs/`:

- `docs/README.md`
- `docs/quickstart/ecef-quickstart.md`
- `docs/guides/blind-solve.md`
- `docs/guides/camera-yml-workflows.md`
- `docs/reference/cli-blind-solve.md`
- `docs/reference/camera-yml-schema.md`
- `docs/operations/performance-troubleshooting.md`

## Prerequisites

On macOS:

```bash
brew install cmake boost astrometry-net exiftool ffmpeg
```

Python helpers used by `tools/blind_solve_from_camera_yml.py` and visualization scripts:

```bash
python3 -m pip install pyyaml numpy
```

Required external tools used by the blind solver:

- `solve-field`
- `wcsinfo`
- `tablist`
- `text2fits`
- `curl`
- `sips` on macOS

## Build

Build from the repository root:

```bash
cd /Users/stdaux-001/StdAux/Projects/ACDS/star_tracker
make -j4
```

Binary path:

```bash
./bin/lost
```

## Astrometry Index Setup

Create the index directory if needed:

```bash
mkdir -p conf/astrometry-index
```

### Targeted downloads

For mixed DSLR and narrower fields, start with a targeted subset instead of downloading everything:

```bash
# Wide-field Tycho-2 indexes
for i in 4119 4118 4117 4116 4115 4114 4113 4112 4111 4110 4109; do
  curl -fL -C - "https://data.astrometry.net/4100/index-${i}.fits" \
    -o "conf/astrometry-index/index-${i}.fits"
done

# Narrower-field 4200 family subset
for i in 4210 4211 4212 4213 4214 4215 4216 4217 4218 4219; do
  curl -fL -C - "https://data.astrometry.net/4200/index-${i}.fits" \
    -o "conf/astrometry-index/index-${i}.fits"
done
```

You can also use the helper script:

```bash
./tools/download-astrometry-indexes.sh 4200 00 11 conf/astrometry-index
```

### Full crawl warning

The full crawler is resume-safe but extremely disk-heavy:

```bash
./tools/download-all-astrometry-indexes.sh conf/astrometry-index
```

Operational notes from the current repo state:

- the crawler discovers all published `4100`, `4200`, `5000`, `6000`, and `6100` families
- a partial 2026-05 run reached about 10 GB after only the early `4200` files
- a later run failed with `No space left on device` while downloading `index-4200-38.fits`
- if a transfer fails mid-file, rerunning the same command resumes with `curl -C -`

Use the full crawl only if you have large free disk headroom. If space is tight, prefer targeted indexes and avoid piping the downloader through `tee`.

## Blind-Solve Quick Start

### Single image

Wide-field starting point:

```bash
./bin/lost blind-solve \
  --profile wide-field \
  --image sample/DSC08982.jpeg \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-wide \
  --ecef-output logs/blind-solve-wide/ecef_attitude.json \
  --overwrite
```

Narrow-field starting point:

```bash
./bin/lost blind-solve \
  --profile narrow-field \
  --image sample/_AST0931.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-narrow \
  --ecef-output logs/blind-solve-narrow/ecef_attitude.json \
  --overwrite
```

Important blind-solve controls:

- `--profile default|wide-field|narrow-field`
- `--scale-low`, `--scale-high`
- `--downsample`
- `--timeout`
- `--preprocess on|off`
- `--preprocess-mode global|tiles`
- `--target-width`, `--max-dim`
- `--tile-width`, `--tile-height`, `--tile-overlap`
- `--max-star-count`
- `--min-star-separation`

Notes:

- `--profile` supplies preset defaults, but any explicit CLI flag overrides the profile value
- `--min-star-separation` is now wired into the real filtered-source pipeline, not just stored as an unused option
- for the current dataset, narrow-field images performed better with `global` preprocessing than `tiles`

### Batch solve

```bash
./bin/lost blind-solve-batch \
  --profile wide-field \
  --input-dir sample \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-batch \
  --jobs 4 \
  --overwrite
```

Batch outputs include:

- `<output-dir>/summary.tsv`
- `<output-dir>/summary_pretty.tsv`
- per-image subdirectories under `<output-dir>/`


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

