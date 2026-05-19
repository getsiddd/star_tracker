# ECEF Quickstart

This quickstart gets you from one image to a solved attitude, ECEF JSON output, and an optional plot.

## 1. Build

```bash
cd /Users/stdaux-001/StdAux/Projects/ACDS/star_tracker
make -j4
```

## 2. Download a usable index subset

```bash
mkdir -p conf/astrometry-index

for i in 4119 4118 4117 4116 4115 4114 4113 4112 4111 4110 4109; do
  curl -fL -C - "https://data.astrometry.net/4100/index-${i}.fits" \
    -o "conf/astrometry-index/index-${i}.fits"
done
```

For full-crawl downloads, see `../operations/performance-troubleshooting.md` first. The full index crawl is large enough to exhaust local disk.

## 3. Run a single-image blind solve with ECEF output

```bash
./bin/lost blind-solve \
  --profile narrow-field \
  --image sample/_AST0931.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-single \
  --ecef on \
  --3d-viz off \
  --ecef-output logs/blind-solve-single/ecef_attitude.json \
  --overwrite
```

If you know the image is much wider, switch to `--profile wide-field`.

## 4. Generate a 3D visualization from the ECEF JSON

```bash
python3 tools/ecef_visualization.py \
  --json logs/blind-solve-single/ecef_attitude.json \
  --output logs/blind-solve-single/ecef_3d.png
```

## 5. Run the single-camera camera.yml workflow

```bash
python3 tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./bin/lost \
  --results-dir logs/blind-solve-cameras/single
```

Optional timeout override:

```bash
python3 tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./bin/lost \
  --results-dir logs/blind-solve-cameras/single \
  --timeout 400
```

## Output locations

- solve artifacts: `logs/blind-solve-single/`
- ECEF JSON: `logs/blind-solve-single/ecef_attitude.json`
- optional plot: `logs/blind-solve-single/ecef_3d.png`
