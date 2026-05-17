# ECEF Quickstart

This quickstart gets you from input image to ECEF JSON and optional 3D plot.

## 1. Build

```bash
cd /Users/stdaux-001/StdAux/Projects/ACDS/star_tracker
cmake -S . -B build
cmake --build build -j4
```

## 2. Run a single-image blind solve with ECEF output

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
  --ecef on \
  --3d-viz off \
  --ecef-output logs/blind-solve-single/ecef_attitude.json \
  --overwrite
```

## 3. Generate 3D visualization from JSON

```bash
/Users/stdaux-001/StdAux/Projects/ACDS/.venv/bin/python tools/ecef_visualization.py \
  --json logs/blind-solve-single/ecef_attitude.json \
  --output logs/blind-solve-single/ecef_3d.png
```

## 4. Run camera.yml workflow (single mode)

```bash
/Users/stdaux-001/StdAux/Projects/ACDS/.venv/bin/python tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/single
```

Optional override:

```bash
/Users/stdaux-001/StdAux/Projects/ACDS/.venv/bin/python tools/blind_solve_from_camera_yml.py \
  --mode single \
  --camera-config conf/camera.yml \
  --binary ./build/bin/lost \
  --results-dir logs/blind-solve-cameras/single \
  --timeout 400
```

## Output locations

- Solve logs and artifacts: `logs/blind-solve-single/`
- ECEF JSON: `logs/blind-solve-single/ecef_attitude.json`
- 3D PNG (if generated): `logs/blind-solve-single/ecef_3d.png`
