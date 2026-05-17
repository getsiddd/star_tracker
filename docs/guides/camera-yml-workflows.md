# camera.yml Workflows

This guide explains mode-based execution from `conf/camera.yml`.

## Modes

- `single`: per-camera image solve via `blind-solve`
- `folder`: per-camera directory solve via `blind-solve-batch`
- `live`: webcam capture + batch solve pipeline

## Default runtime behavior

Current mode defaults in `conf/camera.yml` include:

- `timeout: 400`
- `target_width: 720`
- `preprocess: on`
- `preprocess_mode: global`

Per-camera values override mode defaults.

## Python runner

```bash
/Users/stdaux-001/StdAux/Projects/ACDS/.venv/bin/python tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/single
```

CLI timeout override for all cameras in the selected mode:

```bash
/Users/stdaux-001/StdAux/Projects/ACDS/.venv/bin/python tools/blind_solve_from_camera_yml.py \
  --mode folder \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/folder \
  --timeout 400
```

## LOST binary wrapper

```bash
./bin/lost camera-yml --mode single --camera-config conf/camera.yml
./bin/lost camera-yml --mode folder --camera-config conf/camera.yml
./bin/lost camera-yml --mode live --camera-config conf/camera.yml
```

## Output layout

- Per-camera: `<results-dir>/<camera_id>/`
- Command used: `<results-dir>/<camera_id>/command.txt`
- Run log: `<results-dir>/<camera_id>/run.log`
- Aggregate table: `<results-dir>/timings.tsv`
