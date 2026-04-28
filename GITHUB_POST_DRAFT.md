Title: Operational blind-solve + batch reporting + multi-camera body-frame workflow improvements from downstream fork

Hello maintainers,

Our team started from UWCubeSat/lost and has developed several operational improvements for blind solve reliability, batch throughput/reporting, and multi-camera body-frame workflows.

Highlights:
- Fast-pass then full-pass blind solve flow with retry cleanup
- Parallel blind-solve-batch with automatic table outputs
- summary.tsv is machine-precision for downstream fusion, plus summary_pretty.tsv for human-readable review
- Added RA/Dec HMS/DMS, field size/radius, and pixel-scale reporting
- Real-time helper pipeline from webcam/video to batch solve
- camera.yml + Wahba/SVD fusion workflow for multi-camera body-frame attitude estimation
- GUI-based satellite-body tracker placement and camera.yml export (with tracker highlighting and local axes)

Recent verification:
- One-image batch test generated both files successfully:
  - logs/verify_summary_20260429_050609/solve/summary.tsv
  - logs/verify_summary_20260429_050609/solve/summary_pretty.tsv

We can contribute this upstream either as:
1. A single consolidated PR, or
2. A split set of focused PRs:
- blind solve reliability/performance
- batch reporting precision + pretty output
- realtime helper scripts
- multi-camera fusion + GUI tooling

If useful, we can provide a cleaned patch series and small reproducible test cases per feature area.

Thanks.