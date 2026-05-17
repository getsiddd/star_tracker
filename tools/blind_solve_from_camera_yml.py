#!/usr/bin/env python3
"""Run blind solve workflows from camera.yml in separate modes.

Supported modes:
- single: per-camera single-image solve (`lost blind-solve`)
- folder: per-camera folder solve (`lost blind-solve-batch`)
- live: per-camera webcam workflow (`tools/realtime_batch_from_video.sh`)

Dependencies:
    pip install pyyaml
"""

from __future__ import annotations

import argparse
import re
import shlex
import subprocess
import time
from pathlib import Path
from typing import Any, Dict, List, Tuple

import yaml


BOOL_TRUE = {"1", "true", "on", "yes", "y"}


def to_on_off(value: Any, default: str) -> str:
    if value is None:
        return default
    if isinstance(value, bool):
        return "on" if value else "off"
    text = str(value).strip().lower()
    return "on" if text in BOOL_TRUE else "off"


def opt(camera: Dict[str, Any], defaults: Dict[str, Any], key: str, fallback: Any) -> Any:
    if key in camera:
        return camera[key]
    if key in defaults:
        return defaults[key]
    return fallback


def build_single_command(
    binary: Path,
    camera: Dict[str, Any],
    defaults: Dict[str, Any],
    index_dir: str,
    output_dir: Path,
) -> List[str]:
    image = str(opt(camera, defaults, "image", "")).strip()
    if not image:
        raise ValueError("missing image path")

    scale_low = str(opt(camera, defaults, "scale_low", 10))
    scale_high = str(opt(camera, defaults, "scale_high", 400))
    downsample = str(opt(camera, defaults, "downsample", 2))
    timeout = str(opt(camera, defaults, "timeout", 400))
    preprocess = to_on_off(opt(camera, defaults, "preprocess", "on"), "on")
    preprocess_mode = str(opt(camera, defaults, "preprocess_mode", "global"))
    target_width = str(opt(camera, defaults, "target_width", 720))
    max_dim = str(opt(camera, defaults, "max_dim", 2200))
    overwrite = to_on_off(opt(camera, defaults, "overwrite", True), "on")

    ecef = to_on_off(opt(camera, defaults, "ecef", "on"), "on")
    viz3d = to_on_off(opt(camera, defaults, "viz_3d", "off"), "off")
    ecef_console = to_on_off(opt(camera, defaults, "ecef_console", "on"), "on")

    viz_output = output_dir / "ecef_3d.png"
    ecef_output = output_dir / "ecef_attitude.json"

    cmd = [
        str(binary),
        "blind-solve",
        "--image", image,
        "--index-dir", index_dir,
        "--output-dir", str(output_dir),
        "--scale-low", scale_low,
        "--scale-high", scale_high,
        "--downsample", downsample,
        "--timeout", timeout,
        "--preprocess", preprocess,
        "--preprocess-mode", preprocess_mode,
        "--target-width", target_width,
        "--max-dim", max_dim,
        "--ecef", ecef,
        "--3d-viz", viz3d,
        "--viz-output", str(viz_output),
        "--ecef-output", str(ecef_output),
        "--ecef-console", ecef_console,
    ]

    if overwrite == "on":
        cmd.append("--overwrite")

    return cmd


def build_batch_command(
    binary: Path,
    camera: Dict[str, Any],
    defaults: Dict[str, Any],
    index_dir: str,
    output_dir: Path,
) -> List[str]:
    input_dir = str(opt(camera, defaults, "input_dir", "")).strip()
    if not input_dir:
        raise ValueError("missing input_dir")

    scale_low = str(opt(camera, defaults, "scale_low", 10))
    scale_high = str(opt(camera, defaults, "scale_high", 400))
    downsample = str(opt(camera, defaults, "downsample", 2))
    timeout = str(opt(camera, defaults, "timeout", 400))
    jobs = str(opt(camera, defaults, "jobs", 2))
    preprocess = to_on_off(opt(camera, defaults, "preprocess", "on"), "on")
    preprocess_mode = str(opt(camera, defaults, "preprocess_mode", "global"))
    target_width = str(opt(camera, defaults, "target_width", 720))
    max_dim = str(opt(camera, defaults, "max_dim", 2200))
    overwrite = to_on_off(opt(camera, defaults, "overwrite", True), "on")

    cmd = [
        str(binary),
        "blind-solve-batch",
        "--input-dir", input_dir,
        "--index-dir", index_dir,
        "--output-dir", str(output_dir),
        "--scale-low", scale_low,
        "--scale-high", scale_high,
        "--downsample", downsample,
        "--timeout", timeout,
        "--jobs", jobs,
        "--preprocess", preprocess,
        "--preprocess-mode", preprocess_mode,
        "--target-width", target_width,
        "--max-dim", max_dim,
    ]

    if overwrite == "on":
        cmd.append("--overwrite")
    return cmd


def build_live_command(
    root_dir: Path,
    camera: Dict[str, Any],
    defaults: Dict[str, Any],
    output_dir: Path,
) -> List[str]:
    index_dir = str(opt(camera, defaults, "index_dir", "conf/astrometry-index"))
    camera_index = str(opt(camera, defaults, "camera_index", 0))
    fps = str(opt(camera, defaults, "fps", 1))
    duration_sec = str(opt(camera, defaults, "duration_sec", 60))
    scale_low = str(opt(camera, defaults, "scale_low", 10))
    scale_high = str(opt(camera, defaults, "scale_high", 400))
    downsample = str(opt(camera, defaults, "downsample", 2))
    timeout = str(opt(camera, defaults, "timeout", 400))
    fast_timeout = str(opt(camera, defaults, "fast_timeout", timeout))
    fallback_timeout = str(opt(camera, defaults, "fallback_timeout", timeout))
    fallback_on_fail = to_on_off(opt(camera, defaults, "fallback_on_fail", "on"), "on")
    solve_every_nth = str(opt(camera, defaults, "solve_every_nth", 1))
    jobs = str(opt(camera, defaults, "jobs", 2))
    target_width = str(opt(camera, defaults, "target_width", 720))

    script = root_dir / "tools" / "realtime_batch_from_video.sh"
    return [
        str(script),
        "--source", "webcam",
        "--camera-index", camera_index,
        "--fps", fps,
        "--duration-sec", duration_sec,
        "--index-dir", index_dir,
        "--output-dir", str(output_dir),
        "--scale-low", scale_low,
        "--scale-high", scale_high,
        "--downsample", downsample,
        "--timeout", timeout,
        "--fast-timeout", fast_timeout,
        "--fallback-timeout", fallback_timeout,
        "--fallback-on-fail", fallback_on_fail,
        "--solve-every-nth", solve_every_nth,
        "--jobs", jobs,
        "--target-width", target_width,
    ]


def parse_result(output_text: str) -> Dict[str, str]:
    result = {
        "solve_success": "0",
        "ra_deg": "NA",
        "dec_deg": "NA",
        "rotation_deg": "NA",
        "plate_scale_arcsec_per_pix": "NA",
    }

    for line in output_text.splitlines():
        parts = line.strip().split(maxsplit=1)
        if len(parts) != 2:
            continue
        key, val = parts[0], parts[1]
        if key == "blind_solve_success":
            result["solve_success"] = val
        elif key == "blind_solve_ra_deg":
            result["ra_deg"] = val
        elif key == "blind_solve_dec_deg":
            result["dec_deg"] = val
        elif key == "blind_solve_rotation_deg":
            result["rotation_deg"] = val
        elif key == "blind_solve_plate_scale_arcsec_per_pix":
            result["plate_scale_arcsec_per_pix"] = val

    return result


def parse_batch_summary(output_text: str) -> Tuple[str, str, str]:
    total = "0"
    solved = "0"
    failed = "0"
    match = re.search(r"batch_summary\s+total=(\d+)\s+solved=(\d+)\s+failed=(\d+)", output_text)
    if match:
        total, solved, failed = match.group(1), match.group(2), match.group(3)
    return total, solved, failed


def run_cmd(cmd: List[str]) -> Tuple[int, str, int]:
    t0 = time.time()
    proc = subprocess.run(cmd, capture_output=True, text=True)
    elapsed = int(round(time.time() - t0))
    combined_output = (proc.stdout or "") + "\n" + (proc.stderr or "")
    return proc.returncode, combined_output, elapsed


def get_mode_section(cfg: Dict[str, Any], mode: str) -> Dict[str, Any]:
    # New structure: solve_modes.<mode>
    solve_modes = cfg.get("solve_modes", {}) or {}
    if mode in solve_modes:
        return solve_modes[mode] or {}

    # Backward compatibility: treat legacy top-level as single mode.
    if mode == "single":
        return {
            "defaults": cfg.get("blind_solve_defaults", {}) or {},
            "cameras": cfg.get("cameras", []) or [],
        }
    return {}


def main() -> int:
    parser = argparse.ArgumentParser(description="Run blind-solve workflows from camera.yml")
    parser.add_argument("--camera-config", default="conf/camera.yml", help="Path to camera.yml")
    parser.add_argument("--binary", default="./build/bin/lost", help="Path to LOST binary")
    parser.add_argument("--results-dir", default="logs/blind-solve-cameras", help="Root output directory")
    parser.add_argument("--timings-tsv", default="", help="Optional timings TSV path")
    parser.add_argument("--mode", choices=["single", "folder", "live"], default="single", help="Execution mode")
    parser.add_argument("--timeout", type=int, default=None, help="Override timeout (seconds) for all solves")
    args = parser.parse_args()

    cfg_path = Path(args.camera_config)
    if not cfg_path.exists():
        raise SystemExit(f"camera config not found: {cfg_path}")

    cfg = yaml.safe_load(cfg_path.read_text()) or {}
    mode_cfg = get_mode_section(cfg, args.mode)
    cameras = mode_cfg.get("cameras", [])
    if not cameras:
        raise SystemExit(f"no cameras found for mode: {args.mode}")

    defaults = mode_cfg.get("defaults", {}) or {}
    # Override timeout if provided via CLI
    if args.timeout is not None:
        defaults["timeout"] = args.timeout
    index_dir = str(defaults.get("index_dir", "conf/astrometry-index"))

    binary = Path(args.binary)
    if not binary.exists():
        raise SystemExit(f"binary not found: {binary}")

    results_dir = Path(args.results_dir)
    results_dir.mkdir(parents=True, exist_ok=True)

    timings_path = Path(args.timings_tsv) if args.timings_tsv else (results_dir / "timings.tsv")
    rows = [
        "mode\tcamera_id\tsource\tsolve_success\telapsed_sec\tra_deg\tdec_deg\ttotal_images\tsolved_images\tfailed_images\tecef_json\tout_dir"
    ]

    print("mode\tcamera_id\tsource\tsolve_success\telapsed_sec\tra_deg\tdec_deg\ttotal_images\tsolved_images\tout_dir")

    for camera in cameras:
        cam_id = str(camera.get("id", "camera_unknown"))
        out_dir = results_dir / cam_id
        out_dir.mkdir(parents=True, exist_ok=True)

        source = ""
        parsed = {"solve_success": "0", "ra_deg": "NA", "dec_deg": "NA"}
        total = "0"
        solved = "0"
        failed = "0"

        if args.mode == "single":
            image = str(opt(camera, defaults, "image", "")).strip()
            if not image:
                print(f"single\t{cam_id}\tMISSING_IMAGE\t0\t0\tNA\tNA\t0\t0\tSKIPPED")
                continue
            source = Path(image).name
            cmd = build_single_command(binary, camera, defaults, index_dir, out_dir)
            code, combined_output, elapsed = run_cmd(cmd)
            parsed = parse_result(combined_output)
            total = "1"
            solved = parsed["solve_success"]
            failed = "0" if solved == "1" else "1"

        elif args.mode == "folder":
            input_dir = str(opt(camera, defaults, "input_dir", "")).strip()
            if not input_dir:
                print(f"folder\t{cam_id}\tMISSING_INPUT_DIR\t0\t0\tNA\tNA\t0\t0\tSKIPPED")
                continue
            source = input_dir
            cmd = build_batch_command(binary, camera, defaults, index_dir, out_dir)
            code, combined_output, elapsed = run_cmd(cmd)
            total, solved, failed = parse_batch_summary(combined_output)
            parsed["solve_success"] = "1" if solved != "0" else "0"

        else:  # live
            camera_index = str(opt(camera, defaults, "camera_index", 0))
            source = f"webcam:{camera_index}"
            root_dir = cfg_path.parent.parent
            cmd = build_live_command(root_dir, camera, defaults, out_dir)
            code, combined_output, elapsed = run_cmd(cmd)
            total, solved, failed = parse_batch_summary(combined_output)
            parsed["solve_success"] = "1" if solved != "0" else "0"

        # Write command/output log for debugging and reproducibility.
        (out_dir / "command.txt").write_text(" ".join(shlex.quote(x) for x in cmd) + "\n")
        (out_dir / "run.log").write_text(combined_output)

        print(
            f"{args.mode}\t{cam_id}\t{source}\t{parsed['solve_success']}\t{elapsed}\t"
            f"{parsed['ra_deg']}\t{parsed['dec_deg']}\t{total}\t{solved}\t{out_dir}"
        )

        rows.append(
            "\t".join(
                [
                    args.mode,
                    cam_id,
                    source,
                    parsed["solve_success"],
                    str(elapsed),
                    parsed["ra_deg"],
                    parsed["dec_deg"],
                    total,
                    solved,
                    failed,
                    str(out_dir / "ecef_attitude.json"),
                    str(out_dir),
                ]
            )
        )

    timings_path.write_text("\n".join(rows) + "\n")
    print(f"\nWrote timings table: {timings_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
