# Upstream Development Update for UWCubeSat/lost

This fork started from UWCubeSat/lost and now includes a set of practical improvements focused on blind solving throughput, reliability, and multi-camera satellite body-frame workflows.

## Why this update may be useful upstream

The changes are aimed at real operations problems:
- Faster and more robust blind solve attempts on hard images
- Better batch automation and reporting for dataset-level runs
- Safer handling around index-file environments and reruns
- A practical workflow for multi-camera body-frame configuration and fusion

## Key additions

1. Blind solve and preprocessing improvements
- Two-stage solve strategy: fast pass first, then full pass fallback
- Reduced redundant retry behavior in full-pass execution path
- Preprocess controls for global downscale and tiles
- Improved index config generation pattern for top-level index directory use

2. Batch throughput and outputs
- Parallel blind-solve-batch execution
- Automatic batch tables on every run
- New precision split:
  - summary.tsv is machine-precision first for fusion and post-processing
  - summary_pretty.tsv is rounded for human review

3. Richer solved metadata
- Added center RA/Dec HMS and DMS values
- Added field size and field radius outputs
- Added pixel scale outputs in summary tables

4. Real-time test runner support
- Scripted webcam or video frame extraction + batch solve path
- More robust handling of frame limiting and safer file naming for downloaded inputs

5. Multi-camera body-frame workflow
- camera.yml template with explicit frame reference metadata
- Fusion utility using Wahba/SVD from per-camera summary outputs
- Browser GUI for placing trackers on a satellite body model and exporting camera.yml
  - Clear selected vs non-selected tracker color styling
  - Inverted horizontal orbit drag behavior
  - Visible local axes for each tracker
  - All coordinates explicitly handled with respect to satellite body frame

6. Path robustness in fusion
- Relative summary paths are resolved relative to camera.yml location, not current working directory

## Recent verification evidence

A one-image batch verification run was executed on sample data and produced both output tables successfully:
- logs/verify_summary_20260429_050609/solve/summary.tsv
- logs/verify_summary_20260429_050609/solve/summary_pretty.tsv

The run emitted:
- batch_summary_file .../summary.tsv
- batch_summary_pretty_file .../summary_pretty.tsv

## Suggested upstream collaboration path

- Open a discussion first with this summary and ask whether maintainers prefer:
  1. A single feature branch PR with guarded flags
  2. A split into focused PRs:
     - blind solve reliability/performance
     - batch reporting precision and pretty output
     - realtime helper scripts
     - multi-camera fusion and GUI tooling

## Proposed short message for upstream issue/discussion

Title:
Operational blind-solve + batch reporting + multi-camera body-frame workflow improvements from downstream fork

Body:
Hello maintainers,

Our team started from UWCubeSat/lost and has developed several operational improvements for blind solve reliability, batch throughput/reporting, and multi-camera body-frame workflows.

Highlights:
- Fast-pass then full-pass blind solve flow with retry cleanup
- Parallel batch solve with automatic tables
- summary.tsv now machine-precision for downstream fusion, plus summary_pretty.tsv for human-readable review
- Added RA/Dec HMS/DMS, field size/radius, and pixel-scale reporting
- Real-time helper pipeline from webcam/video to batch solve
- camera.yml + fusion workflow for multi-camera body-frame attitude estimation
- GUI-based satellite-body tracker placement and camera.yml export

We can contribute this upstream either as one PR or split into smaller thematic PRs, based on maintainer preference.

If useful, we can provide a cleaned patch series and minimal test cases for each feature area.

Thanks.
