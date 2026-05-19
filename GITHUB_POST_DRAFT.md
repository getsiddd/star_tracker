Title: Blind-solve profiles, filtered-source solving, centralized CLI options, and refreshed operations docs

Hello maintainers,

This downstream fork now includes a more operational blind-solve workflow, a cleaner CLI option model, and refreshed user-facing documentation for running and tuning the solver.

Highlights:
- real filtered-source blind solve path, so `min-star-separation` and `max-star-count` affect the actual solve input
- `--profile` presets for `blind-solve` and `blind-solve-batch` (`default`, `wide-field`, `narrow-field`)
- profile application that preserves explicit CLI overrides
- centralized live CLI option declarations in `include/options.hpp`
- batch solve outputs with `summary.tsv` and `summary_pretty.tsv`
- refreshed README and `docs/` instructions covering build, targeted index downloads, camera.yml workflows, and performance tuning

Operational notes now documented in-repo:
- narrow-field images in the current dataset performed better with global preprocessing than tile mode
- full astrometry index crawl is resume-safe, but can consume large disk space quickly
- targeted index downloads remain the recommended starting point for normal use

Recent validation artifacts:
- baseline sample benchmark: `logs/benchmark-all-samples-20260518/summary.tsv`
- profile tuning outputs under `logs/profile-tuning-20260518/`

This can be proposed upstream either as:
1. one consolidated PR for the blind-solve and docs workflow
2. split PRs for CLI/profile work, filtered-source solving, and docs/operations updates

Thanks.