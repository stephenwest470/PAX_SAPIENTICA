"""Per-model profiles (mean ± SD over replicates) for a batch output folder, e.g. SimulationResults/Yayoi-Sim-v2.

Usage: python Scripts/ProfileModels.py SimulationResults/Yayoi-Sim-v2
Writes Profiles.tsv (one row per model) and Profiles.md (Table 1 layout plus per-model profiles) into that folder.
Targets come from the folder's Expected.tsv / Zones.tsv (copied there by the batch runner).
"""
import csv, glob, math, os, re, statistics as st, sys

out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "SimulationResults/Yayoi-Sim-v2")
rd = lambda p: list(csv.DictReader(open(p, encoding="utf-8-sig"), delimiter="\t"))
kv = lambda p: {r[0]: r[1] for r in csv.reader(open(p, encoding="utf-8-sig"), delimiter="\t") if len(r) >= 2}
batch = rd(os.path.join(out, "Batch.tsv"))
expected = kv(os.path.join(out, "Expected.tsv"))
zones = rd(os.path.join(out, "Zones.tsv"))
TARGET = float(expected["population"]); ANC = float(expected["toraijin_ratio"]); THR = float(expected["farming_threshold"])
lo, hi = (int(x) for x in expected.get("study_districts", "1-72").split("-"))
study = set(range(lo, hi + 1))
zone_of = {int(d): z["key"] for z in zones for d in z["districts"].split(",")}
label = lambda y: "–" if y is None or (isinstance(y, float) and math.isnan(y)) else (f"{round(1 - y)} BC" if y < 0.5 else f"AD {round(y)}")


def run_metrics(model_dir, run_dir):
    summ = kv(os.path.join(run_dir, "RunSummary.tsv"))
    P, F, S, Y, ZP, ZF = {}, {}, {}, {}, {}, {}
    for r in csv.DictReader(open(os.path.join(run_dir, "DistrictTimeSeries.tsv"), encoding="utf-8"), delimiter="\t"):
        d = int(r["district"])
        if d not in study: continue
        s = int(r["step"]); p, f = int(r["population"]), int(r["farmers"])
        P[s] = P.get(s, 0) + p; F[s] = F.get(s, 0) + f; S[s] = S.get(s, 0) + float(r["snp_sum"]); Y[s] = float(r["astronomical_year"])
        z = zone_of.get(d)
        if z: ZP[z, s] = ZP.get((z, s), 0) + p; ZF[z, s] = ZF.get((z, s), 0) + f
    steps = sorted(P); last = steps[-1]
    seed = summ.get("seed", "")
    logs = glob.glob(os.path.join(model_dir, f"run_*_seed_{seed}.log"))
    lp = {}
    if logs:
        lp = {int(m.group(1)): int(m.group(2)) for m in re.finditer(r"^step (\d+)/\d+ .*?population (\d+)", open(logs[0], encoding="utf-8", errors="replace").read(), re.M)}
    m = {"seed": seed, "stop": summ.get("stop_reason", ""), "end_year": Y[last], "population": P[last],
         "pct_target": 100 * P[last] / TARGET, "ancestry_pct": 100 * S[last] / P[last], "farmers_pct": 100 * F[last] / P[last],
         "jomon_left": P[last] - F[last],
         "pre_migration_jomon_growth": 100 * math.log(lp[1800] / lp[360]) / 120 if 360 in lp and 1800 in lp else float("nan"),
         "reached_target": next((Y[s] for s in steps if P[s] >= TARGET), float("nan")), "level": 0}
    for z in zones:
        a = next((Y[s] for s in steps if ZP.get((z["key"], s), 0) and ZF[z["key"], s] / ZP[z["key"], s] >= THR), float("nan"))
        m["arr_" + z["key"]] = a
        final = ZP.get((z["key"], last), 0)
        if final and ZF[z["key"], last] / final >= THR: m["level"] = max(m["level"], int(z["level"]))
    return m


NUM = ["population", "pct_target", "ancestry_pct", "farmers_pct", "jomon_left", "level", "pre_migration_jomon_growth", "reached_target"] + ["arr_" + z["key"] for z in zones]
profiles = []
for b in batch:
    mid = b["id"]; model_dir = os.path.join(out, mid)
    runs = [run_metrics(model_dir, os.path.dirname(p)) for p in sorted(glob.glob(os.path.join(model_dir, "*", "RunSummary.tsv")))
            if os.path.exists(os.path.join(os.path.dirname(p), "DistrictTimeSeries.tsv"))]
    prof = {"id": mid, "description": b["description"], "n": len(runs), "stops": ",".join(sorted({r["stop"] for r in runs})),
            "end": ",".join(sorted({label(r["end_year"]) for r in runs}))}
    for k in NUM:
        v = [r[k] for r in runs if not (isinstance(r[k], float) and math.isnan(r[k]))]
        prof[k + "_mean"] = st.mean(v) if v else float("nan")
        prof[k + "_sd"] = st.stdev(v) if len(v) > 1 else float("nan")
        prof[k + "_n"] = len(v)
    prof["runs"] = runs
    profiles.append(prof)

cols = ["id", "description", "n", "stops", "end"] + [f"{k}_{s}" for k in NUM for s in ("mean", "sd", "n")]
with open(os.path.join(out, "Profiles.tsv"), "w", encoding="utf-8", newline="\n") as f:
    f.write("\t".join(cols) + "\n")
    for p in profiles:
        f.write("\t".join("" if isinstance(p[c], float) and math.isnan(p[c]) else (f"{p[c]:.4g}" if isinstance(p[c], float) else str(p[c])) for c in cols) + "\n")

ms = lambda p, k, d=1, scale=1.0: "–" if math.isnan(p[k + "_mean"]) else (f"{p[k + '_mean'] * scale:.{d}f}" + ("" if math.isnan(p[k + "_sd"]) else f" ± {p[k + '_sd'] * scale:.{d}f}"))
L = [f"# Model profiles: {os.path.basename(out)}", "",
     f"Mean ± SD over replicates. Population and migrant ancestry are for districts {lo}-{hi} at the last step (AD 50 unless a run stopped early).",
     f"Lv. = farthest zone with farmers ≥ {THR * 100:.0f}% at the last step. Expected: population {TARGET:,.0f} (Crema et al. 2024), ancestry {ANC * 100:.0f}%, Lv. {expected['farming_level']}.", "",
     "| @ | Model | n | Population (万人) | % of target | Migrant ancestry (%) | Lv. | Farmers (%) | Jomon left | Jomon growth before migrants (%/yr) |",
     "|---|---|---|---|---|---|---|---|---|---|",
     f"| A | 予想 (expected) | | {TARGET / 1e4:.1f} | 100 | {ANC * 100:.0f} | {expected['farming_level']} | | | |"]
for p in profiles:
    L.append(f"| {p['id']} | {p['description']} | {p['n']} | {ms(p, 'population', 1, 1e-4)} | {ms(p, 'pct_target', 0)} | {ms(p, 'ancestry_pct', 1)} | "
             f"{ms(p, 'level', 1)} | {ms(p, 'farmers_pct', 1)} | {ms(p, 'jomon_left', 0)} | {ms(p, 'pre_migration_jomon_growth', 2)} |")
L += ["", "## Farming arrival (mean year farmers first reach the threshold; runs reaching it / runs)", "",
      "| @ | " + " | ".join(z["ja-JP"] for z in zones) + " |", "|---|" + "---|" * len(zones),
      "| Fujio (2009) | " + " | ".join(label(float(z["fujio_year"]) + 1) if z["fujio_year"] else "–" for z in zones) + " |"]
for p in profiles:
    L.append(f"| {p['id']} | " + " | ".join(
        (label(p['arr_' + z['key'] + '_mean']) + ("" if math.isnan(p['arr_' + z['key'] + '_sd']) else f" ± {p['arr_' + z['key'] + '_sd']:.0f}y")
         if p['arr_' + z['key'] + '_n'] else "not reached") + f" ({p['arr_' + z['key'] + '_n']}/{p['n']})" for z in zones) + " |")
L += ["", "## Runs", "", "| @ | seed | stop | end | population | ancestry (%) | Lv. |", "|---|---|---|---|---|---|---|"]
for p in profiles:
    for r in p["runs"]:
        L.append(f"| {p['id']} | {r['seed']} | {r['stop']} | {label(r['end_year'])} | {r['population']:,} | {r['ancestry_pct']:.1f} | {r['level']} |")
open(os.path.join(out, "Profiles.md"), "w", encoding="utf-8", newline="\n").write("\n".join(L) + "\n")
print("\n".join(L[:len(profiles) + 8]))
