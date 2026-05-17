# camera.yml Reference

This reference describes the solve-related parts of `conf/camera.yml`.

## Top-level solve section

```yaml
solve_modes:
  single:
    defaults: ...
    cameras: ...
  folder:
    defaults: ...
    cameras: ...
  live:
    defaults: ...
    cameras: ...
```

## Common default keys

- `index_dir`: Astrometry index directory
- `scale_low`, `scale_high`: plate scale search bounds
- `downsample`: solve-field downsampling
- `timeout`: CPU time limit per solve attempt (seconds)
- `target_width`: global preprocessing width target

## Mode-specific keys

single defaults:
- `preprocess`, `preprocess_mode`, `max_dim`
- `ecef`, `viz_3d`, `ecef_console`
- `overwrite`

folder defaults:
- `jobs`
- `preprocess`, `preprocess_mode`, `max_dim`
- `overwrite`

live defaults:
- `jobs`
- `fps`
- `duration_sec`

## Camera entries

single camera entry:

```yaml
- id: cam0_single
  image: sample/_AST0931.JPG
```

folder camera entry:

```yaml
- id: cam0_folder
  input_dir: sample
```

live camera entry:

```yaml
- id: cam0_live
  camera_index: 0
```

## Precedence

When resolving a value:

1. camera-level key
2. mode defaults key
3. script fallback

Current script fallback timeout is 400.
