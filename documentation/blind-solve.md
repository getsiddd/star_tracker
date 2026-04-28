# Blind Solve (Astrometry Style)

This project now supports blind solving from image-only input using astrometry.net.

## What this mode solves

- Unknown focal length
- Unknown pixel size
- Unknown pointing direction

Outputs:

- RA / Dec
- Rotation angle
- Plate scale (arcsec/pixel)
- WCS file path

## 1. Download precomputed quad index files

Use the helper script:

```sh
cd star_tracker
./tools/download-astrometry-indexes.sh 4200 00 11 conf/astrometry-index
```

This downloads:

- `index-4200-00.fits`
- ...
- `index-4200-11.fits`

You can change the series/range depending on your expected image scales.

## 2. Solve an image with unknown camera parameters

```sh
cd star_tracker
./bin/lost blind-solve \
  --image sample/image.png \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve \
  --scale-low 0.1 \
  --scale-high 120 \
  --downsample 2 \
  --timeout 120 \
  --overwrite
```

## 3. Output fields printed by `lost`

- `blind_solve_success`
- `blind_solve_ra_deg`
- `blind_solve_dec_deg`
- `blind_solve_rotation_deg`
- `blind_solve_plate_scale_arcsec_per_pix`
- `blind_solve_wcs_file`

## Environment fallback

If `--index-dir` is not passed, the solver checks `ASTROMETRY_INDEX_DIR`.
