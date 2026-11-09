"""Playback videos of each model's AVERAGE result: every province (kuni) is filled with the mean of its replicates.

For each model in <batch>/Batch.tsv, the complete runs in <batch>/<id>/ are averaged per province and per logged step
(every 10 years):
  population   mean population over the runs (a run with nobody in the province counts as 0)
  subsistence  mean farmer share over the runs that have people there
  snp          mean migrant-derived ancestry over the runs that have people there
  mtdna        mean of each run's mean mtDNA haplogroup index (MapViewer key 3) over the runs that have people there
The maps, colour scales, decade interpolation and calendar come from ReplaySimulation.py (province mode), so the
videos look like the per-run replays. Only the calendar is drawn.

Usage (from the repository root):
  python Scripts/ReplayAverages.py SimulationResults/Yaponesia2026
Options: --models B,P  --modes population,subsistence,mtdna,snp  --fps 24  --scale 8  --jobs 3  --force  --lang en|ja
Writes <batch>/Averages/<id>/Videos/<mode>.mp4 and <batch>/Averages/<id>/AverageDistrictTimeSeries.tsv
(the averaged values behind the videos). Requires numpy, Pillow and ffmpeg on PATH.
"""

from __future__ import annotations

import argparse
import csv
import sys
import warnings
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ReplaySimulation as R  # noqa: E402

MODES = ["population", "subsistence", "mtdna", "snp"]


def load_run(run: Path, mtdna_index: dict[str, int]):
    """Per-step arrays (256 provinces) of population, farmer share, ancestry and mean mtDNA index (NaN = nobody)."""
    pop, share, snp, mt = {}, {}, {}, {}
    for r in csv.DictReader(open(run / "DistrictTimeSeries.tsv", encoding="utf-8"), delimiter="\t"):
        s, d = int(r["step"]), int(r["district"])
        p = float(r["population"])
        for table in (pop, share, snp):
            table.setdefault(s, np.full(256, np.nan))
        pop[s][d] = p
        if p > 0:
            share[s][d] = float(r["farmers"]) / p
            snp[s][d] = float(r["snp_sum"]) / p
    logged = {}
    for row in R.read_tsv(run / "mtDNA.txt")[1:]:
        values = np.full(256, np.nan)
        for d, cell in enumerate(row[3:-1], start=1):
            total = acc = 0
            for part in cell.split("/"):
                if ":" in part:
                    name, count = part.split(":")
                    total += int(count)
                    acc += mtdna_index.get(name, 0) * int(count)
            if total:
                values[d] = acc / total
        logged[int(row[0])] = values
    order = sorted(logged)
    for s in pop:
        # the simulator's mtDNA log stops one output interval before the end: carry the last entry forward
        earlier = [k for k in order if k <= s]
        mt[s] = logged[earlier[-1] if earlier else order[0]] if order else np.full(256, np.nan)
    return pop, share, snp, mt


def average_model(model_dir: Path, out_dir: Path):
    runs = sorted(p.parent for p in model_dir.glob("*/RunSummary.tsv") if (p.parent / "DistrictTimeSeries.tsv").exists())
    if not runs:
        return None
    model = R.key_values(runs[0] / "RunInfo.tsv").get("model", "Yaponesia")
    names = R.read_tsv(R.ROOT / "Data" / "Simulations" / model / "mtDNA_List.tsv")[1:]
    mtdna_index = {n[0]: i for i, n in enumerate(names)}
    data = [load_run(r, mtdna_index) for r in runs]
    steps = sorted(set.intersection(*(set(d[0]) for d in data)))
    avg = {}
    with np.errstate(all="ignore"), warnings.catch_warnings():
        warnings.simplefilter("ignore", RuntimeWarning)  # provinces with nobody in any run
        for s in steps:
            pops = np.stack([d[0][s] for d in data])
            present = np.nan_to_num(pops) > 0
            mean = lambda k: np.nanmean(np.stack([d[k][s] for d in data]), axis=0)  # runs with people only
            avg[s] = {"population": np.nanmean(np.where(np.isnan(pops), 0.0, pops), axis=0),
                      "subsistence": mean(1), "snp": mean(2), "mtdna": mean(3), "runs": present.sum(axis=0)}
    info = R.key_values(runs[0] / "RunInfo.tsv")
    start_jd, spy = int(info["start_julian_day"]), int(info["steps_per_year"])
    out_dir.mkdir(parents=True, exist_ok=True)
    with open(out_dir / "AverageDistrictTimeSeries.tsv", "w", encoding="utf-8", newline="\n") as f:
        f.write("step\tastronomical_year\tdistrict\tpopulation_mean\tfarmer_share_mean\tsnp_mean\tmtdna_index_mean\truns_with_people\n")
        for s in steps:
            year = R.julian_date(start_jd + s * 365.25 / spy)[0]
            a = avg[s]
            for d in np.nonzero(~np.isnan(a["population"]) & (a["population"] > 0))[0]:
                fmt = lambda v: "" if np.isnan(v) else f"{v:.4f}"
                f.write(f"{s}\t{year}\t{d}\t{a['population'][d]:.1f}\t{fmt(a['subsistence'][d])}\t{fmt(a['snp'][d])}\t"
                        f"{fmt(a['mtdna'][d])}\t{a['runs'][d]}\n")
    return runs, steps, avg, start_jd, spy


def values(base: R.Base, a: dict, mode: str) -> np.ndarray:
    """Province values on the MapViewer 0..MAX_VIS scale, as ReplaySimulation.district_frames computes them."""
    v = np.full(256, np.nan)
    pop = a["population"]
    have = ~np.isnan(pop) & (pop > 0)
    if mode == "population":
        density = pop[have] / np.maximum(1.0, base.land_cells[np.nonzero(have)[0]] * R.CELL_KM2)
        v[have] = R.MAX_VIS * np.clip(np.log10(np.maximum(density, 1e-3) / 0.1) / 3.0, 0, 1)
    elif mode in ("subsistence", "snp"):
        v[have] = a[mode][have] * R.MAX_VIS
    elif mode == "mtdna":
        v[have] = a["mtdna"][have] / R.MTDNA_SCALE * R.MAX_VIS
    return v


def render_model(model_dir: Path, out_dir: Path, modes: list[str], fps: int, scale: int, force: bool, lang: str = "en") -> list[str]:
    result = average_model(model_dir, out_dir)
    if result is None:
        return [f"{model_dir.name}: no complete runs"]
    runs, steps, avg, start_jd, spy = result
    base = R.Base(R.district_raster(runs[0], R.area_of(runs[0])), scale)
    calendar = R.Calendar(base.width, lang)
    per_decade = 4
    done = [f"{model_dir.name}: averaged {len(runs)} runs over {len(steps)} steps"]
    for mode in modes:
        out = out_dir / "Videos" / f"{mode}.mp4"
        if out.exists() and not force:
            done.append(f"  {out.relative_to(R.ROOT)} (exists)")
            continue

        def frames():
            previous = None
            for s in steps:
                jd, value = start_jd + s * 365.25 / spy, values(base, avg[s], mode)
                if previous is not None:
                    pjd, pvalue = previous
                    for k in range(per_decade):
                        t = k / per_decade
                        v = np.where(np.isnan(pvalue), value, np.where(np.isnan(value), pvalue, pvalue + (value - pvalue) * t))
                        pil = Image.fromarray(R.paint_districts(base, v, mode))
                        calendar.draw(pil, pjd + (jd - pjd) * t)
                        yield np.asarray(pil.convert("RGB"))
                previous = (jd, value)
            if previous is not None:
                pil = Image.fromarray(R.paint_districts(base, previous[1], mode))
                calendar.draw(pil, previous[0])
                for _ in range(fps):  # hold the final state for one second
                    yield np.asarray(pil.convert("RGB"))

        R.encode(frames(), base.width, base.height, fps, out)
        done.append(f"  {out.relative_to(R.ROOT)}")
    return done


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("batch", help="batch output folder, e.g. SimulationResults/Yaponesia2026")
    parser.add_argument("--models", default="", help="comma-separated model ids (default: all in Batch.tsv)")
    parser.add_argument("--modes", default=",".join(MODES))
    parser.add_argument("--fps", type=int, default=24)
    parser.add_argument("--scale", type=int, default=8, help="simulation cells per output pixel")
    parser.add_argument("--jobs", type=int, default=3)
    parser.add_argument("--force", action="store_true", help="re-render videos that already exist")
    parser.add_argument("--lang", choices=["en", "ja"], default="en", help="calendar language")
    args = parser.parse_args()
    batch = Path(args.batch).resolve()
    modes = [m for m in args.modes.split(",") if m]
    if set(modes) - set(MODES):
        sys.exit(f"unknown mode(s): {', '.join(sorted(set(modes) - set(MODES)))}")
    ids = [r[0] for r in R.read_tsv(batch / "Batch.tsv")[1:]]
    if args.models:
        ids = [i for i in ids if i in args.models.split(",")]
    with ProcessPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = {pool.submit(render_model, batch / i, batch / "Averages" / i, modes, args.fps, args.scale, args.force, args.lang): i for i in ids}
        for future, i in futures.items():
            try:
                print("\n".join(future.result()), flush=True)
            except Exception as e:  # keep going with the other models
                print(f"FAILED {i}: {e}", flush=True)


if __name__ == "__main__":
    main()
