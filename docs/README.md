# Star Tracker Documentation

This folder contains project documentation organized by purpose.

## Structure

- [quickstart/ecef-quickstart.md](quickstart/ecef-quickstart.md): Fast path to run ECEF output and 3D visualization.
- [guides/blind-solve.md](guides/blind-solve.md): End-to-end blind solve guide for single image and batch usage.
- [guides/camera-yml-workflows.md](guides/camera-yml-workflows.md): How to run single, folder, and live modes from camera.yml.
- [guides/ecef-attitude-guide.md](guides/ecef-attitude-guide.md): ECEF attitude concepts, outputs, and integration notes.
- [reference/cli-blind-solve.md](reference/cli-blind-solve.md): Reference for solve-related CLI options.
- [reference/camera-yml-schema.md](reference/camera-yml-schema.md): Reference for camera.yml mode defaults and keys.
- [operations/performance-troubleshooting.md](operations/performance-troubleshooting.md): Why solving can be slow and how to tune runtime.

## Current Defaults (as of 2026-05-17)

- camera.yml mode defaults use `timeout: 400`.
- Global preprocessing target width is `720` for camera.yml workflows.
- `target_width` is passed in single, folder, and live pipelines.

## Recommended Read Order

1. [quickstart/ecef-quickstart.md](quickstart/ecef-quickstart.md)
2. [guides/blind-solve.md](guides/blind-solve.md)
3. [guides/camera-yml-workflows.md](guides/camera-yml-workflows.md)
4. [operations/performance-troubleshooting.md](operations/performance-troubleshooting.md)
