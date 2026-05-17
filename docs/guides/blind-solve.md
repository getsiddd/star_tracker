# Blind Solve Guide

This guide covers single-image and batch blind solving with current recommended settings.

## Prerequisites

- `solve-field` and `wcsinfo` available from astrometry.net.
- Astrometry index files downloaded into `conf/astrometry-index`.

## Download index files

Example helper script:

```bash
./tools/download-astrometry-indexes.sh 4200 00 11 conf/astrometry-index
```

For wider coverage across fields of view, include additional 4100/4200 families.

## Single image solve

```bash
./build/bin/lost blind-solve \
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
  --overwrite
```

## Batch folder solve

```bash
./build/bin/lost blind-solve-batch \
  --input-dir sample \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-batch \
  --scale-low 10 --scale-high 400 \
  --downsample 2 \
  --timeout 400 \
  --jobs 2 \
  --preprocess on \
  --preprocess-mode global \
  --target-width 720 \
  --max-dim 2200 \
  --overwrite
```

## Key output fields

- `blind_solve_success`
- `blind_solve_ra_deg`
- `blind_solve_dec_deg`
- `blind_solve_rotation_deg`
- `blind_solve_plate_scale_arcsec_per_pix`
- `blind_solve_wcs_file`

## Timeout guidance

- `--timeout 400` is the documented default in current camera.yml workflows.
- Some hard images can exceed this budget and still solve if timeout is removed.
- Timeout failures are not always index failures; test with `--timeout 0` to confirm.
