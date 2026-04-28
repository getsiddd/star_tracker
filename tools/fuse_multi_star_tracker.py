#!/usr/bin/env python3
"""
Fuse multiple camera star-tracker results into one body attitude estimate.

Method:
1) For each camera, read the latest solved row from summary.tsv.
2) Convert solved (ra_deg, dec_deg) into an inertial boresight unit vector.
3) Use configured boresight_body vectors from camera.yml as body observations.
4) Solve Wahba via SVD: R_b_to_i minimizes ||v_i - R * v_b||.
5) Convert fused boresight to RA/Dec for quick downstream use.

Dependencies:
  pip install pyyaml numpy
"""

from __future__ import annotations

import argparse
import csv
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

import numpy as np
import yaml


def deg2rad(x: float) -> float:
    return x * math.pi / 180.0


def rad2deg(x: float) -> float:
    return x * 180.0 / math.pi


def normalize(v: np.ndarray) -> np.ndarray:
    n = np.linalg.norm(v)
    if n <= 0.0:
        raise ValueError("zero-length vector")
    return v / n


def ra_dec_to_unit(ra_deg: float, dec_deg: float) -> np.ndarray:
    ra = deg2rad(ra_deg)
    dec = deg2rad(dec_deg)
    c = math.cos(dec)
    return np.array([c * math.cos(ra), c * math.sin(ra), math.sin(dec)], dtype=float)


def unit_to_ra_dec(v: np.ndarray) -> Tuple[float, float]:
    v = normalize(v)
    ra = math.atan2(v[1], v[0])
    if ra < 0:
        ra += 2.0 * math.pi
    dec = math.asin(max(-1.0, min(1.0, v[2])))
    return rad2deg(ra), rad2deg(dec)


def load_latest_solved(summary_tsv: Path) -> Optional[Dict[str, str]]:
    if not summary_tsv.exists():
        return None

    rows = []
    with summary_tsv.open("r", newline="") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            rows.append(row)

    solved = [r for r in rows if str(r.get("result", "")).strip().lower() == "solved"]
    if not solved:
        return None
    return solved[-1]


def wahba_svd(body_vectors: List[np.ndarray], inertial_vectors: List[np.ndarray], weights: List[float]) -> np.ndarray:
    if len(body_vectors) != len(inertial_vectors):
        raise ValueError("body/inertial vector count mismatch")
    if len(body_vectors) < 2:
        raise ValueError("need at least 2 vector observations")

    b_mat = np.zeros((3, 3), dtype=float)
    for vb, vi, w in zip(body_vectors, inertial_vectors, weights):
        b_mat += float(w) * np.outer(vi, vb)

    u, _, vt = np.linalg.svd(b_mat)
    m = np.diag([1.0, 1.0, np.linalg.det(u) * np.linalg.det(vt)])
    r = u @ m @ vt
    return r


def main() -> int:
    parser = argparse.ArgumentParser(description="Fuse multiple star tracker outputs using Wahba/SVD")
    parser.add_argument("--camera-config", default="conf/camera.yml", help="Path to camera.yml")
    parser.add_argument("--output", default="logs/fusion_result.txt", help="Fusion result output file")
    args = parser.parse_args()

    cfg_path = Path(args.camera_config)
    if not cfg_path.exists():
        raise SystemExit(f"missing camera config: {cfg_path}")

    cfg = yaml.safe_load(cfg_path.read_text())
    cameras = cfg.get("cameras", [])
    if not cameras:
        raise SystemExit("camera.yml has no cameras")

    min_cameras = int(cfg.get("fusion", {}).get("min_cameras", 2))
    weighting = str(cfg.get("fusion", {}).get("weighting", "inverse_pixel_scale"))

    body_vectors: List[np.ndarray] = []
    inertial_vectors: List[np.ndarray] = []
    weights: List[float] = []
    used: List[Tuple[str, float, float, float]] = []

    for cam in cameras:
        cam_id = str(cam.get("id", "unknown"))
        boresight_body = np.array(cam.get("boresight_body", []), dtype=float)
        if boresight_body.shape != (3,):
            continue

        summary_tsv_raw = Path(str(cam.get("summary_tsv", "")))
        if summary_tsv_raw.is_absolute():
            summary_tsv = summary_tsv_raw
        else:
            summary_tsv = cfg_path.parent / summary_tsv_raw
        row = load_latest_solved(summary_tsv)
        if row is None:
            continue

        try:
            ra_deg = float(row.get("ra_deg", "nan"))
            dec_deg = float(row.get("dec_deg", "nan"))
            pixel_scale = float(row.get("pixel_scale_arcsec_per_pix", row.get("scale_arcsec_per_pix", "nan")))
        except ValueError:
            continue

        if math.isnan(ra_deg) or math.isnan(dec_deg):
            continue

        w = 1.0
        if weighting == "inverse_pixel_scale" and pixel_scale > 0.0 and not math.isnan(pixel_scale):
            w = 1.0 / pixel_scale

        body_vectors.append(normalize(boresight_body))
        inertial_vectors.append(ra_dec_to_unit(ra_deg, dec_deg))
        weights.append(w)
        used.append((cam_id, ra_deg, dec_deg, pixel_scale))

    if len(body_vectors) < min_cameras:
        raise SystemExit(f"insufficient solved cameras: {len(body_vectors)} < {min_cameras}")

    r_b_to_i = wahba_svd(body_vectors, inertial_vectors, weights)

    # Report fused boresight for body +X axis.
    body_x = np.array([1.0, 0.0, 0.0], dtype=float)
    fused_boresight_i = r_b_to_i @ body_x
    fused_ra, fused_dec = unit_to_ra_dec(fused_boresight_i)

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)

    lines = []
    lines.append("fused_attitude_method Wahba_SVD")
    lines.append(f"used_cameras {len(used)}")
    for cam_id, ra, dec, ps in used:
        lines.append(f"camera {cam_id} ra_deg={ra:.6f} dec_deg={dec:.6f} pixel_scale={ps:.6f}")
    lines.append(f"fused_ra_deg {fused_ra:.6f}")
    lines.append(f"fused_dec_deg {fused_dec:.6f}")
    lines.append("R_b_to_i")
    for i in range(3):
        lines.append(f"{r_b_to_i[i,0]: .9f} {r_b_to_i[i,1]: .9f} {r_b_to_i[i,2]: .9f}")

    out_path.write_text("\n".join(lines) + "\n")
    print(out_path)
    print("\n".join(lines))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
