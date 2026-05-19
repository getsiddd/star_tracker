# CLI Reference (Solve Commands)

This reference reflects the current options declared in `include/options.hpp`.

## blind-solve

Defaults before profile application:

- `--profile default`
- `--image <path>`
- `--index-dir <path>`
- `--output-dir logs/blind-solve`
- `--scale-low 0.1`
- `--scale-high 120`
- `--downsample 2`
- `--timeout 120`
- `--preprocess on`
- `--preprocess-mode global`
- `--target-width 720`
- `--max-dim 2200`
- `--tile-width 1800`
- `--tile-height 1800`
- `--tile-overlap 300`
- `--overwrite off`
- `--ecef on`
- `--3d-viz on`
- `--viz-output <path>`
- `--ecef-output <path>`
- `--ecef-console on`
- `--max-star-count 50`
- `--min-star-separation 0.1`

Profile aliases:

- `default`, `generic`, `auto`
- `wide`, `wide-field`
- `narrow`, `narrow-field`

Profile behavior:

- profiles apply after CLI parsing
- explicit flags win over profile defaults

## blind-solve-batch

Defaults before profile application:

- `--profile default`
- `--input-dir <path>`
- `--index-dir <path>`
- `--output-dir logs/blind-solve-batch`
- `--scale-low 0.1`
- `--scale-high 120`
- `--downsample 2`
- `--timeout 120`
- `--jobs <hardware concurrency>`
- `--preprocess on`
- `--preprocess-mode global`
- `--target-width 720`
- `--max-dim 2200`
- `--tile-width 1800`
- `--tile-height 1800`
- `--tile-overlap 300`
- `--overwrite off`
- `--max-star-count 50`
- `--min-star-separation 0.1`

The same `wide-field` and `narrow-field` profile semantics apply here.

## camera-yml

Wrapper options:

- `--mode single|folder|live`
- `--camera-config conf/camera.yml`
- `--results-dir logs/blind-solve-cameras`
- `--timings-tsv <path>`
- `--python python3`
- `--run-fusion`
- `--fusion-output logs/fusion_result.txt`

Notes:

- `camera-yml` itself does not expose `--profile`
- use `conf/camera.yml` numeric fields to emulate the current profile presets when needed
