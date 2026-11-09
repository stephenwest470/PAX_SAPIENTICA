"""Replay PAXS_BatchSimulator runs as videos, one per map mode, from the files each run writes.

Map modes match the MapViewer's settlement display (keys 1-6) and use its colour scale
(Library/PAX_MAHOROBA/Rendering/SimulationColor.hpp). Only the calendar is drawn as UI.

  population   settlement population (MapViewer key 1)
  subsistence  farmer share (key 2)
  mtdna        mean mtDNA haplogroup index (key 3)
  snp          migrant-derived ancestry (key 4)
  language     majority language (key 5)
  bronze       bronze holdings (key 6, settlement snapshots only)

Two sources, chosen automatically per run:
  SettlementSnapshots.bin  every settlement every 10 years, drawn as circles like the MapViewer.
                           Written by PAXS_BatchSimulator from 2026-10-18 onwards.
  district logs            older runs only logged district totals (DistrictTimeSeries.tsv, mtDNA.txt,
                           Language.txt), so each province is filled with its value instead.

Usage (from the repository root):
  python Scripts/ReplaySimulation.py SimulationResults/Yayoi-Sim            # every complete run in a batch
  python Scripts/ReplaySimulation.py SimulationResults/Yayoi-Sim/B/2026-...  # one run
Options: --modes population,snp  --fps 24  --scale 8  --force  --jobs 2  --source auto|snapshots|districts  --lang en|ja
Videos are written to <run>/Videos/<mode>.mp4. Requires numpy, Pillow and ffmpeg on PATH.
"""

from __future__ import annotations

import argparse
import csv
import math
import struct
import subprocess
import sys
from concurrent.futures import ProcessPoolExecutor
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent.parent
TILE = 256
ZOOM = 10
CELL_KM2 = (40075.0 / (2 ** 18)) ** 2 * math.cos(math.radians(36)) ** 2  # ~125 m cells at Japan's latitude

ALL_MODES = ["population", "subsistence", "mtdna", "snp", "language", "bronze"]

# MapViewer constants (SettlementRenderer.hpp)
MAX_VIS = 75.0
MTDNA_SCALE = 27.0
SETTLEMENT_COLORS = np.array([
    (45, 87, 154), (36, 92, 156), (27, 96, 158), (18, 101, 161), (9, 105, 163), (0, 110, 165), (0, 111, 164),
    (0, 112, 163), (0, 112, 161), (0, 113, 160), (0, 114, 159), (0, 115, 158), (0, 117, 157), (0, 118, 156),
    (0, 120, 155), (0, 121, 154), (0, 125, 148), (0, 130, 143), (0, 134, 137), (0, 139, 132), (0, 143, 126),
    (0, 141, 115), (0, 138, 104), (0, 136, 94), (0, 133, 83), (0, 131, 72), (6, 136, 60), (13, 141, 48),
    (19, 145, 37), (26, 150, 25), (32, 155, 13), (55, 159, 16), (78, 164, 19), (101, 168, 23), (124, 173, 26),
    (147, 177, 29), (156, 182, 26), (166, 187, 24), (175, 193, 21), (185, 198, 19), (194, 203, 16), (201, 204, 13),
    (208, 205, 10), (216, 205, 6), (223, 206, 3), (230, 207, 0), (228, 202, 1), (226, 197, 2), (225, 191, 3),
    (221, 181, 5), (223, 186, 4), (219, 176, 6), (218, 171, 7), (216, 165, 8), (215, 160, 9), (213, 155, 10),
    (210, 138, 12), (206, 122, 15), (203, 105, 17), (199, 89, 20), (196, 72, 22), (193, 64, 28), (189, 56, 34),
    (186, 48, 40), (182, 40, 46), (179, 32, 52), (178, 31, 57), (176, 30, 62), (175, 29, 68), (173, 28, 73),
    (172, 27, 78), (170, 26, 83), (169, 25, 88), (167, 24, 92), (166, 23, 97), (164, 22, 102), (131, 18, 82),
    (98, 13, 61), (66, 9, 41), (33, 4, 20), (0, 0, 0), (255, 255, 255)], dtype=np.uint8)
LANGUAGE_COLORS = np.array([(99, 99, 99), (0, 131, 72), (45, 87, 154), (182, 40, 46), (230, 207, 0)], dtype=np.uint8)

SEA = np.array((170, 200, 222), dtype=np.uint8)
LAND = np.array((236, 233, 224), dtype=np.uint8)       # land with no settlements / no data
BORDER = np.array((120, 118, 110), dtype=np.uint8)
SNAPSHOT_DTYPE = np.dtype([("x", "<u2"), ("y", "<u2"), ("pop", "<u2"), ("farm", "<u2"),
                           ("snp", "u1"), ("mtdna", "u1"), ("lang", "u1"), ("bronze", "u1")])


# ------------------------------------------------------------------------------------------------
# Inputs
# ------------------------------------------------------------------------------------------------

def read_tsv(path: Path) -> list[list[str]]:
    return [line.split("\t") for line in path.read_text(encoding="utf-8-sig").splitlines() if line]


def key_values(path: Path) -> dict[str, str]:
    return {r[0]: r[1] for r in read_tsv(path)[1:] if len(r) >= 2}


def area_of(run: Path) -> tuple[int, int, int, int]:
    """Start and end z10 tile of the run's simulation area (first matching key in Area.tsv)."""
    name = key_values(run / "Settings.tsv").get("area", "Japan")
    for r in read_tsv(ROOT / "Data" / "Simulations" / "Area.tsv")[1:]:
        if r[0] == name:
            return int(r[1]), int(r[2]), int(r[3]), int(r[4])
    raise ValueError(f"area {name} not in Area.tsv")


def decode_tile(data: bytes) -> np.ndarray:
    """Run-length encoded u8 tile (Input8BitBinary): <=253 literal, 255 v n8 / 254 v n16 = n+1 copies of v."""
    out = np.zeros(TILE * TILE, dtype=np.uint8)
    i = j = 0
    n = len(data)
    while i < n and j < TILE * TILE:
        b = data[i]
        if b <= 253:
            out[j] = b
            i += 1
            j += 1
        else:
            value = data[i + 1]
            length = (data[i + 2] if b == 255 else (data[i + 2] << 8) + data[i + 3]) + 1
            out[j:j + length] = value
            j += length
            i += 3 if b == 255 else 4
    return out.reshape(TILE, TILE)


def district_raster(run: Path, area: tuple[int, int, int, int]) -> np.ndarray:
    """District ID per simulation cell over the run's area (0 = sea)."""
    model = key_values(run / "RunInfo.tsv").get("model", "Yaponesia")
    tiles_dir = None
    for r in read_tsv(ROOT / "Data" / "Simulations" / model / "MapList.tsv")[1:]:
        if r[0] == "district":
            tiles_dir = ROOT / r[3]
    if tiles_dir is None:
        raise ValueError(f"no district map in Data/Simulations/{model}/MapList.tsv")
    sx, sy, ex, ey = area
    raster = np.zeros(((ey - sy + 1) * TILE, (ex - sx + 1) * TILE), dtype=np.uint8)
    for ty in range(sy, ey + 1):
        for tx in range(sx, ex + 1):
            f = tiles_dir / f"zxy_{ZOOM}_{tx}_{ty}.bin"
            if f.exists():
                raster[(ty - sy) * TILE:(ty - sy + 1) * TILE, (tx - sx) * TILE:(tx - sx + 1) * TILE] = decode_tile(f.read_bytes())
    return raster


def julian_date(jd: float) -> tuple[int, int, int]:
    """Julian-calendar (astronomical year, month, day) of a Julian day number."""
    z = int(jd + 0.5)
    b = z + 1524
    c = int((b - 122.1) / 365.25)
    d = int(365.25 * c)
    e = int((b - d) / 30.6001)
    day = b - d - int(30.6001 * e)
    month = e - 1 if e < 14 else e - 13
    year = c - 4716 if month > 2 else c - 4715
    return year, month, day


# ------------------------------------------------------------------------------------------------
# Frames
# ------------------------------------------------------------------------------------------------

def scale_index(values: np.ndarray) -> np.ndarray:
    return np.clip(values, 0, MAX_VIS).astype(np.int64)


class Base:
    """Sea / land / province-border background at the output resolution."""

    def __init__(self, raster: np.ndarray, scale: int):
        h, w = raster.shape
        h -= h % (2 * scale)
        w -= w % (2 * scale)
        self.scale = scale
        self.small = raster[:h:scale, :w:scale]
        self.height, self.width = self.small.shape
        img = np.empty((self.height, self.width, 3), dtype=np.uint8)
        img[:] = SEA
        img[self.small > 0] = LAND
        border = np.zeros_like(self.small, dtype=bool)
        border[:, 1:] |= (self.small[:, 1:] != self.small[:, :-1]) & (self.small[:, 1:] > 0) & (self.small[:, :-1] > 0)
        border[1:, :] |= (self.small[1:, :] != self.small[:-1, :]) & (self.small[1:, :] > 0) & (self.small[:-1, :] > 0)
        self.border = border
        self.image = img
        # land cells per district (full resolution) for densities
        self.land_cells = np.bincount(raster.ravel(), minlength=256)


MONTHS = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]


class Calendar:
    def __init__(self, width: int, lang: str = "en"):
        self.lang = lang
        size = max(16, width // 48)
        font_dir = ROOT / "Data" / "Font" / "noto-sans-jp"
        self.font = ImageFont.truetype(str(font_dir / "NotoSansJP-Medium.otf"), size)
        self.small = ImageFont.truetype(str(font_dir / "NotoSansJP-Regular.otf"), int(size * 0.7))
        self.pad = size // 2

    def draw(self, img: Image.Image, jd: float) -> None:
        y, m, d = julian_date(jd)
        if self.lang == "ja":
            ja = f"紀元前{1 - y}年 {m}月{d}日" if y <= 0 else f"西暦{y}年 {m}月{d}日"
            en = f"{1 - y} BCE" if y <= 0 else f"{y} CE"
            lines = [(ja, self.font), (f"ユリウス暦  {en}  ·  JD {int(jd):,}", self.small)]
        else:
            year = f"{1 - y} BCE" if y <= 0 else f"{y} CE"
            date = f"{year}, {MONTHS[m - 1]}, {d}"  # year, month, day
            lines = [(date, self.font), (f"Julian calendar  ·  JD {int(jd):,}", self.small)]
        draw = ImageDraw.Draw(img, "RGBA")
        widths = [draw.textbbox((0, 0), t, font=f)[2] for t, f in lines]
        heights = [draw.textbbox((0, 0), t, font=f)[3] for t, f in lines]
        box_w = max(widths) + 2 * self.pad
        box_h = sum(heights) + 3 * self.pad // 2 + self.pad // 2
        draw.rounded_rectangle((self.pad, self.pad, self.pad + box_w, self.pad + box_h), radius=self.pad // 2,
                               fill=(252, 252, 251, 225), outline=(11, 11, 11, 40))
        ty = self.pad + self.pad // 2
        for (text, font), h in zip(lines, heights):
            draw.text((2 * self.pad, ty), text, font=font, fill=(11, 11, 11, 255))
            ty += h + self.pad // 2


def district_frames(run: Path, base: Base, mode: str):
    """(julian day, image array) per decade, from the district logs. Values are interpolated in render()."""
    info = key_values(run / "RunInfo.tsv")
    start_jd = int(info["start_julian_day"])
    spy = int(info["steps_per_year"])
    steps: dict[int, dict[int, list[float]]] = {}
    for r in csv.DictReader(open(run / "DistrictTimeSeries.tsv", encoding="utf-8"), delimiter="\t"):
        steps.setdefault(int(r["step"]), {})[int(r["district"])] = [float(r["population"]), float(r["farmers"]), float(r["snp_sum"])]

    extra: dict[int, dict[int, float]] = {}
    if mode in ("mtdna", "language"):
        file = run / ("mtDNA.txt" if mode == "mtdna" else "Language.txt")
        model = key_values(run / "RunInfo.tsv").get("model", "Yaponesia")
        names = read_tsv(ROOT / "Data" / "Simulations" / model / "mtDNA_List.tsv")[1:]
        index = {n[0]: i for i, n in enumerate(names)}
        for row in read_tsv(file)[1:]:
            step = int(row[0])
            values = {}
            for d, cell in enumerate(row[3:-1], start=1):
                if mode == "mtdna":
                    total = acc = 0
                    for part in cell.split("/"):
                        if ":" in part:
                            name, count = part.split(":")
                            total += int(count)
                            acc += index.get(name, 0) * int(count)
                    values[d] = acc / total if total else -1.0
                else:
                    try:
                        values[d] = float(cell)
                    except ValueError:
                        values[d] = -1.0
            extra[step] = values

    logged = sorted(extra)
    for step in sorted(steps):
        districts = steps[step]
        if mode in ("mtdna", "language") and step not in extra and logged:
            # the simulator's own logs stop one output interval before the end: carry the last one forward
            earlier = [s for s in logged if s <= step]
            extra[step] = extra[earlier[-1] if earlier else logged[0]]
        value = np.full(256, np.nan)
        for d, (pop, farm, snp) in districts.items():
            if pop <= 0:
                continue
            if mode == "population":
                density = pop / max(1.0, base.land_cells[d] * CELL_KM2)  # people per km²
                value[d] = MAX_VIS * np.clip(math.log10(max(density, 1e-3) / 0.1) / 3.0, 0, 1)  # 0.1–100 /km², log
            elif mode == "subsistence":
                value[d] = farm / pop * MAX_VIS
            elif mode == "snp":
                value[d] = snp / pop * MAX_VIS
            elif mode == "mtdna":
                v = extra.get(step, {}).get(d, -1.0)
                value[d] = v / MTDNA_SCALE * MAX_VIS if v >= 0 else np.nan
            elif mode == "language":
                v = extra.get(step, {}).get(d, -1.0)
                value[d] = round(v) if v >= 0 else np.nan
        yield start_jd + step * 365.25 / spy, value


def paint_districts(base: Base, value: np.ndarray, mode: str) -> np.ndarray:
    img = base.image.copy()
    v = value[base.small]
    have = ~np.isnan(v) & (base.small > 0)
    if mode == "language":
        idx = np.clip(np.nan_to_num(v, nan=0).astype(np.int64), 0, len(LANGUAGE_COLORS) - 1)
        img[have] = LANGUAGE_COLORS[idx[have]]
    else:
        img[have] = SETTLEMENT_COLORS[scale_index(np.nan_to_num(v, nan=0))[have]]
    img[base.border] = BORDER
    return img


def snapshot_frames(path: Path):
    data = path.read_bytes()
    assert data[:8] == b"PAXSSNP1", "not a settlement snapshot file"
    start_jd, spy, _, _ = struct.unpack_from("<IIii", data, 8)
    offset = 24
    while offset + 8 <= len(data):
        step, count = struct.unpack_from("<II", data, offset)
        offset += 8
        if offset + 12 * count > len(data):
            break  # incomplete last frame (run interrupted)
        yield start_jd + step * 365.25 / spy, np.frombuffer(data, dtype=SNAPSHOT_DTYPE, count=count, offset=offset)
        offset += 12 * count


def paint_settlements(base: Base, settlements: np.ndarray, mode: str) -> np.ndarray:
    img = base.image.copy()
    img[base.border] = BORDER
    pop = settlements["pop"].astype(np.float64)
    keep = pop > 0
    s = settlements[keep]
    pop = pop[keep]
    if mode == "language":
        colors = LANGUAGE_COLORS[np.clip(s["lang"], 0, len(LANGUAGE_COLORS) - 1)]
    else:
        if mode == "population":
            v = pop
        elif mode == "subsistence":
            v = s["farm"] / pop * MAX_VIS
        elif mode == "mtdna":
            v = s["mtdna"] / MTDNA_SCALE * MAX_VIS
        elif mode == "snp":
            v = s["snp"] / 255.0 * MAX_VIS
        else:
            v = s["bronze"].astype(np.float64)
        colors = SETTLEMENT_COLORS[scale_index(v)]
    pil = Image.fromarray(img)
    draw = ImageDraw.Draw(pil)
    # MapViewer radius is 1 + population / 10 screen pixels; scaled here to the output resolution
    radius = np.maximum(0.8, (1.0 + pop / 10.0) * 0.45)
    xs = s["x"] / base.scale
    ys = s["y"] / base.scale
    order = np.argsort(pop)  # draw large settlements last
    for k in order:
        r = radius[k]
        draw.ellipse((xs[k] - r, ys[k] - r, xs[k] + r, ys[k] + r), fill=tuple(int(c) for c in colors[k]))
    return np.asarray(pil)


# ------------------------------------------------------------------------------------------------
# Video
# ------------------------------------------------------------------------------------------------

def encode(frames, width: int, height: int, fps: int, out: Path) -> None:
    out.parent.mkdir(parents=True, exist_ok=True)
    tmp = out.with_suffix(".partial.mp4")
    cmd = ["ffmpeg", "-y", "-loglevel", "error", "-f", "rawvideo", "-pix_fmt", "rgb24", "-s", f"{width}x{height}",
           "-r", str(fps), "-i", "-", "-c:v", "libx264", "-preset", "medium", "-crf", "20", "-pix_fmt", "yuv420p",
           "-movflags", "+faststart", str(tmp)]
    proc = subprocess.Popen(cmd, stdin=subprocess.PIPE)
    try:
        for frame in frames:
            proc.stdin.write(frame.tobytes())
    finally:
        proc.stdin.close()
        code = proc.wait()
    if code != 0:
        tmp.unlink(missing_ok=True)
        raise RuntimeError(f"ffmpeg failed ({code}) for {out}")
    tmp.replace(out)


def render_run(run: Path, modes: list[str], fps: int, scale: int, force: bool, source: str, lang: str = "en") -> list[str]:
    snap = run / "SettlementSnapshots.bin"
    use_snapshots = (source == "snapshots") or (source == "auto" and snap.exists())
    if use_snapshots and not snap.exists():
        return [f"{run}: no SettlementSnapshots.bin"]
    area = area_of(run)
    base = Base(district_raster(run, area), scale)
    calendar = Calendar(base.width, lang)
    frames_per_decade = 4
    done = []
    for mode in modes:
        if mode == "bronze" and not use_snapshots:
            continue  # not in the district logs
        out = run / "Videos" / f"{mode}.mp4"
        if out.exists() and not force:
            done.append(f"{out.relative_to(ROOT)} (exists)")
            continue

        def frames():
            if use_snapshots:
                frame = None
                for jd, settlements in snapshot_frames(snap):
                    pil = Image.fromarray(paint_settlements(base, settlements, mode))
                    calendar.draw(pil, jd)
                    frame = np.asarray(pil.convert("RGB"))
                    for _ in range(frames_per_decade):
                        yield frame
                for _ in range(fps if frame is not None else 0):  # hold the final state for one second
                    yield frame
            else:
                previous = None
                for jd, value in district_frames(run, base, mode):
                    if previous is not None:
                        pjd, pvalue = previous
                        for k in range(frames_per_decade):
                            t = k / frames_per_decade
                            v = pvalue + (value - pvalue) * t if mode != "language" else (pvalue if t < 0.5 else value)
                            v = np.where(np.isnan(pvalue), value, np.where(np.isnan(value), pvalue, v)) if mode != "language" else v
                            pil = Image.fromarray(paint_districts(base, v, mode))
                            calendar.draw(pil, pjd + (jd - pjd) * t)
                            yield np.asarray(pil.convert("RGB"))
                    previous = (jd, value)
                if previous is not None:
                    pil = Image.fromarray(paint_districts(base, previous[1], mode))
                    calendar.draw(pil, previous[0])
                    for _ in range(fps):  # hold the final state for one second
                        yield np.asarray(pil.convert("RGB"))

        encode(frames(), base.width, base.height, fps, out)
        done.append(str(out.relative_to(ROOT)))
    return done


def complete_runs(path: Path) -> list[Path]:
    if (path / "RunSummary.tsv").exists():
        return [path]
    return sorted(p.parent for p in path.rglob("RunSummary.tsv"))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("paths", nargs="+", help="batch output folder(s) or run folder(s)")
    parser.add_argument("--modes", default=",".join(ALL_MODES))
    parser.add_argument("--fps", type=int, default=24)
    parser.add_argument("--scale", type=int, default=8, help="simulation cells per output pixel")
    parser.add_argument("--source", choices=["auto", "snapshots", "districts"], default="auto")
    parser.add_argument("--force", action="store_true", help="re-render videos that already exist")
    parser.add_argument("--jobs", type=int, default=2)
    parser.add_argument("--lang", choices=["en", "ja"], default="en", help="calendar language")
    args = parser.parse_args()
    modes = [m for m in args.modes.split(",") if m]
    unknown = set(modes) - set(ALL_MODES)
    if unknown:
        sys.exit(f"unknown mode(s): {', '.join(sorted(unknown))}")

    runs = [r for p in args.paths for r in complete_runs(Path(p).resolve())]
    if not runs:
        sys.exit("no complete runs found")
    print(f"{len(runs)} run(s), modes: {', '.join(modes)}", flush=True)
    with ProcessPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        futures = {pool.submit(render_run, r, modes, args.fps, args.scale, args.force, args.source, args.lang): r for r in runs}
        for future, run in futures.items():
            try:
                for line in future.result():
                    print(line, flush=True)
            except Exception as e:  # keep going with the other runs
                print(f"FAILED {run}: {e}", flush=True)


if __name__ == "__main__":
    main()
