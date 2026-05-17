# CLI Reference (Solve Commands)

## blind-solve

Core inputs:

- `--image <path>`
- `--index-dir <path>`
- `--output-dir <path>`

Solve tuning:

- `--scale-low <float>`
- `--scale-high <float>`
- `--downsample <int>`
- `--timeout <sec>`

Preprocessing:

- `--preprocess on|off`
- `--preprocess-mode global|tiles`
- `--target-width <px>`
- `--max-dim <px>`
- `--tile-width <px>`
- `--tile-height <px>`
- `--tile-overlap <px>`

ECEF and visualization:

- `--ecef on|off`
- `--ecef-output <path>`
- `--ecef-console on|off`
- `--3d-viz on|off`
- `--viz-output <path>`

Behavior:

- `--overwrite`

## blind-solve-batch

Core inputs:

- `--input-dir <path>`
- `--index-dir <path>`
- `--output-dir <path>`

Batch controls:

- `--jobs <int>`
- `--timeout <sec>`

Preprocessing and solve tuning flags are similar to single mode.

## camera-yml

- `--mode single|folder|live`
- `--camera-config <path>`
- `--run-fusion` (optional)
- `--fusion-output <path>` (optional)
