# camera.yml Workflows

This guide explains how `conf/camera.yml` drives single-image, folder, and live workflows.

## Modes

- `single`: per-camera `blind-solve`
- `folder`: per-camera `blind-solve-batch`
- `live`: webcam capture plus solve pipeline via `tools/realtime_batch_from_video.sh`

## Current defaults in `conf/camera.yml`

Single mode defaults:

- `timeout: 400`
- `preprocess: on`
- `preprocess_mode: global`
- `target_width: 720`
- `max_dim: 2200`
- `ecef: on`
- `viz_3d: off`

Folder mode defaults:

- `timeout: 400`
- `jobs: 2`
- `preprocess: on`
- `preprocess_mode: global`
- `target_width: 720`
- `max_dim: 2200`

Live mode defaults:

- `timeout: 400`
- `fast_timeout: 25`
- `fallback_timeout: 120`
- `fallback_on_fail: on`
- `solve_every_nth: 2`
- `jobs: 2`
- `fps: 1`
- `duration_sec: 60`
- `target_width: 720`

Per-camera keys override the matching mode defaults.

## Python runner

```bash
python3 tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./bin/lost \
  --results-dir logs/blind-solve-cameras/single
```

Override timeout for the selected mode at runtime:

```bash
python3 tools/blind_solve_from_camera_yml.py \
  --mode folder \
  --camera-config conf/camera.yml \
  --binary ./bin/lost \
  --results-dir logs/blind-solve-cameras/folder \
  --timeout 400
```

## LOST binary wrapper

```bash
./bin/lost camera-yml --mode single --camera-config conf/camera.yml
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml
./bin/lost camera-yml --mode live --camera-config conf/camera.yml
```

## Important limitation

The current camera.yml runner does not expose blind-solve `--profile`. If you want profile-like behavior in camera-driven workflows, set the explicit numeric fields in `conf/camera.yml`.

## Output layout

- per-camera directory: `<results-dir>/<camera_id>/`
- exact command used: `<results-dir>/<camera_id>/command.txt`
- run log: `<results-dir>/<camera_id>/run.log`
- aggregate timings: `<results-dir>/timings.tsv`

Live mode with fallback enabled also produces:

- `solve_fast/summary.tsv`
- `solve_fallback/summary.tsv`
- `solve/summary_merged.tsv`
