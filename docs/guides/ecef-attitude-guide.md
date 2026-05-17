# ECEF Attitude Guide

This guide explains what ECEF outputs represent and how they are produced by LOST.

## What the system does

- Solves star field attitude using astrometry.
- Converts orientation products into ECEF frame outputs.
- Exports machine-readable JSON and optional visualization-ready data.

## Main implementation files

- `include/ecef-attitude.hpp`
- `src/ecef-attitude.cpp`
- `tools/ecef_visualization.py`

## Output model

The ECEF output includes:

- Quaternion in ECEF frame: `real, i, j, k`
- Euler angles in radians: `ra, dec, roll`
- Axis directions in ECEF: `x_axis, y_axis, z_axis`
- Geodetic location and ECEF position
- Solve metadata (`ra_deg`, `dec_deg`, rotation, pixel scale, WCS path)

## Typical run path

1. `lost blind-solve` estimates star-frame attitude.
2. Time terms (Julian date and GMST) are computed.
3. Attitude is transformed into ECEF.
4. JSON and optional console/plot outputs are produced.

## CLI controls

For `blind-solve`:

- `--ecef on|off`
- `--ecef-output <path>`
- `--ecef-console on|off`
- `--3d-viz on|off`
- `--viz-output <path>`

## Notes on correctness

- If solve fails, no valid ECEF attitude should be interpreted.
- If solve succeeds but appears noisy, verify image quality and star count.
- For reproducible runs, keep scale bounds and preprocessing settings fixed.
