# Performance and Troubleshooting

## Why processing can take a long time

Blind solving is expensive because the solver must match observed star patterns against large index catalogs. Runtime increases when:

- image quality is poor (blur, low contrast, noise)
- too few or too many detected stars are used
- scale search range is wide
- index coverage is broad and large
- timeout is high or disabled

## Timeout vs index problem

Use this decision flow:

1. Run a failing image with `--timeout 0`.
2. If it solves, the prior failure was timeout-driven.
3. If it still does not solve, inspect index coverage and scale bounds.

## Practical tuning

- Start with a profile that matches the field of view.
- Keep preprocessing on and use global mode first.
- Narrow `scale_low` and `scale_high` whenever approximate FOV is known.
- Keep `downsample` moderate unless star density is extremely high.
- Increase timeout only after scale bounds and indexes are credible.
- Use `max-star-count` and `min-star-separation` to keep the filtered-source catalog sane.

Current useful presets:

- `wide-field`: `scale 20..400`, `downsample 2`, `timeout 180`, `target-width 720`
- `narrow-field`: `scale 15..90`, `downsample 1`, `timeout 240`, `target-width 1280`

For live-mode robustness and lower latency:

- Use two-pass solves: small `fast_timeout` first, then retry failed frames with `fallback_timeout`.
- Use frame decimation with `solve_every_nth` so not every captured frame is solved.
- Keep fallback enabled (`fallback_on_fail: on`) to preserve solve rate while maintaining fast average throughput.

## Current benchmark snapshot

The baseline sample run in `logs/benchmark-all-samples-20260518/summary.tsv` solved 3 of 8 inputs.

Important cases:

- `DSC08982.jpeg`: solved quickly
- `img_7660.png`: solved quickly
- `_AST0931.JPG`: solved, but slowly
- `_AST0938.JPG`: remained unsolved under the baseline
- `Stacked_158_Ton 618_10.0s_IRCUT_20260425-214138.jpeg`: failed after long runtime

Use that file as the before-state when measuring tuning changes.

## Narrow-field caveat from current testing

Tile preprocessing looked plausible for narrow images, but it regressed the current dataset badly. The narrow-field preset therefore stays on `preprocess-mode global`.

## Full index download caveat

`tools/download-all-astrometry-indexes.sh` is resume-safe but very large.

Observed operational behavior from the current repo run:

- the script crawls all published `4100`, `4200`, `5000`, `6000`, and `6100` families
- a partial run consumed about 10 GB before even finishing the early `4200` set
- one resumed run ultimately failed with `No space left on device` while downloading `index-4200-38.fits`

Implications:

- prefer targeted downloads unless you know you need the full catalog set
- if the crawl fails mid-transfer, rerun the same command to resume
- avoid extra log piping when disk space is already tight

## Validation checklist

- Confirm index files are readable and not corrupted.
- Check per-image logs in each output directory.
- Verify that preprocessed image dimensions match expectations.
- Re-run one hard case without timeout to classify failure root cause.
