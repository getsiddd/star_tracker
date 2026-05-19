# Star Tracker Documentation

This folder contains the maintained user-facing documentation for the current LOST command surface.

## Structure

- [quickstart/ecef-quickstart.md](quickstart/ecef-quickstart.md): quickest path from image to blind solve, ECEF JSON, and optional visualization.
- [guides/blind-solve.md](guides/blind-solve.md): recommended single-image and batch workflows, including profile usage.
- [guides/camera-yml-workflows.md](guides/camera-yml-workflows.md): mode-based workflows driven from `conf/camera.yml`.
- [guides/ecef-attitude-guide.md](guides/ecef-attitude-guide.md): ECEF outputs and interpretation notes.
- [reference/cli-blind-solve.md](reference/cli-blind-solve.md): current solve-related CLI options and defaults.
- [reference/camera-yml-schema.md](reference/camera-yml-schema.md): current `camera.yml` keys, precedence, and limitations.
- [operations/performance-troubleshooting.md](operations/performance-troubleshooting.md): runtime tuning, failure classification, and index download caveats.

## Source Of Truth

- solve and camera CLI flags: `include/options.hpp`
- profile behavior: `src/lost.cpp`
- blind-solve runtime behavior: `src/blind-solve.cpp`
- camera workflow defaults: `conf/camera.yml`

## Current Defaults (as of 2026-05-19)

- the built binary is `./bin/lost`
- `blind-solve` and `blind-solve-batch` both support `--profile`
- `camera.yml` workflows still use explicit numeric fields rather than `--profile`
- camera.yml defaults still use `target_width: 720` and `timeout: 400`

## Recommended Read Order

1. [quickstart/ecef-quickstart.md](quickstart/ecef-quickstart.md)
2. [guides/blind-solve.md](guides/blind-solve.md)
3. [reference/cli-blind-solve.md](reference/cli-blind-solve.md)
4. [guides/camera-yml-workflows.md](guides/camera-yml-workflows.md)
5. [operations/performance-troubleshooting.md](operations/performance-troubleshooting.md)
