# Blind Solve Guide

This guide covers the current recommended workflows for `blind-solve` and `blind-solve-batch`.

## Prerequisites

- `solve-field`, `wcsinfo`, `tablist`, and `text2fits` available from `astrometry.net`
- index FITS files available in `conf/astrometry-index`
- built binary at `./bin/lost`

## Use targeted indexes first

Start with targeted index families that match your expected field of view. Avoid the full crawler unless you have large free disk space.

Helper script example:

```bash
./tools/download-astrometry-indexes.sh 4200 00 11 conf/astrometry-index
```

## Single-image solves

### Wide-field starting point

```bash
./bin/lost blind-solve \
  --profile wide-field \
  --image sample/DSC08982.jpeg \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-wide \
  --overwrite
```

### Narrow-field starting point

```bash
./bin/lost blind-solve \
  --profile narrow-field \
  --image sample/_AST0931.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-narrow \
  --overwrite
```

### Manual tuning on top of a profile

```bash
./bin/lost blind-solve \
  --profile narrow-field \
  --image sample/_AST0938.JPG \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-ast0938 \
  --scale-low 20 \
  --scale-high 40 \
  --timeout 240 \
  --overwrite
```

Explicit CLI flags override the values supplied by `--profile`.

## Batch solves

```bash
./bin/lost blind-solve-batch \
  --profile wide-field \
  --input-dir sample \
  --index-dir conf/astrometry-index \
  --output-dir logs/blind-solve-batch \
  --jobs 4 \
  --overwrite
```

Important batch outputs:

- `<output-dir>/summary.tsv`
- `<output-dir>/summary_pretty.tsv`
- one artifact directory per input image

## Solve controls that matter most

- `--profile`: `default`, `wide-field`, `narrow-field`
- `--scale-low`, `--scale-high`: narrow these whenever approximate plate scale is known
- `--timeout`: hard cap for each solve invocation
- `--downsample`: faster, but can lose weak sources if pushed too far
- `--preprocess` and `--preprocess-mode`
- `--target-width` and `--max-dim`
- `--max-star-count`
- `--min-star-separation`

## Source filtering behavior

The current solver path now does all of the following before the final filtered solve:

1. extracts the initial source list with `solve-field --dont-augment --keep-xylist`
2. dumps the xy list with `tablist`
3. filters sources in C++
4. rebuilds a FITS table with `text2fits`
5. runs the solve on the filtered source catalog

That means `--min-star-separation` and `--max-star-count` now materially change the actual solve inputs.

## Dataset-specific observations

- `_AST0931.JPG` solved with the narrow-field profile and improved slightly over baseline
- `_AST0938.JPG` remained unsolved, but narrowing the scale window reduced wasted runtime
- tile mode was a poor fit for the current narrow-field sample set; `global` preprocessing remained the better default

## Timeout guidance

- camera.yml workflows still default to `timeout: 400`
- the raw blind-solve CLI defaults to `timeout 120` before profile application
- test a hard failure with `--timeout 0` if you need to separate timeout pressure from outright no-match behavior
