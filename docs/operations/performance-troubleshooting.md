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

- Keep preprocessing on and use `target_width: 720` for large inputs.
- Narrow `scale_low` and `scale_high` when approximate FOV is known.
- Keep `downsample` moderate (for example 2) unless star density is very high.
- Start with `timeout 400`; increase only for difficult images.

For live-mode robustness and lower latency:

- Use two-pass solves: small `fast_timeout` first, then retry failed frames with `fallback_timeout`.
- Use frame decimation with `solve_every_nth` so not every captured frame is solved.
- Keep fallback enabled (`fallback_on_fail: on`) to preserve solve rate while maintaining fast average throughput.

## Validation checklist

- Confirm index files are readable and not corrupted.
- Check per-image logs in each output directory.
- Verify that preprocessed image dimensions match expectations.
- Re-run one hard case without timeout to classify failure root cause.
