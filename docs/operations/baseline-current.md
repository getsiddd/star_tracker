# Baseline Results (Before Robustness Changes)

Date: 2026-05-17

This file captures known output tables before implementing the new robustness/real-time strategy.

## Existing Timing Tables

### `logs/benchmark-sample/timings.tsv`
- rows: 9
- solved: 5
- failed: 4
- total elapsed: 2311 s

### `logs/benchmark-sample-rerun/timings.tsv`
- rows: 9
- solved: 5
- failed: 4
- total elapsed: 1746 s

### `logs/benchmark-sample-default720/timings.tsv`
- rows: 9
- solved: 5
- failed: 4
- total elapsed: 3502 s

### `logs/benchmark-sample-default720-direct/timings.tsv`
- rows: 3
- solved: 1
- failed: 2
- total elapsed: 1345 s

### `logs/blind-solve-cameras/timings.tsv`
- rows: 2
- solved: 0
- failed: 2
- total elapsed: 1821 s

## Baseline Notes

- A previously failing case (`sample/_AST0931.JPG`) solved when timeout was removed (`--timeout 0`), indicating timeout pressure rather than pure index failure for that case.
- Current camera.yml defaults are expected to use timeout 400 and target width 720 for mode workflows.

## Comparison Plan

After robustness changes, rerun:

1. `logs/benchmark-sample` benchmark loop
2. camera.yml single/folder/live runs
3. a known hard case with and without timeout constraints

and compare:

- solve rate
- median/total solve time
- timeout failure count
- ECEF output completeness
