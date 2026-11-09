# Yayoi-Sim-v2: agricultural transition in Japan (CAA Australasia & Japan 2026)

Model inputs for West, S. and Kasuga, H. (2026) "Simulating the Agricultural Transition in Japan:
A Yayoi Period Application of an Open-Source Agent-Based Simulation GIS", CAA Australasia & Japan 2026.
The models extend Kasuga & West (2024, IPSJ Symposium on Humanities and Computer 2024: 217–224).

The simulation library is unchanged from upstream `develop` at d17a8a55c; only input files and the
headless runner (`Projects/BatchSimulator`) and helper scripts (`Scripts/`) are added.

## Base model (B)

| Setting | Value |
| --- | --- |
| Timeframe | 1101 BCE – 50 CE, 13,800 monthly steps |
| Area | 72 provinces of Honshu, Shikoku and Kyushu; 125 m cells |
| Initial population | 76,800 Jomon foragers (Koyama 1978) |
| Settlements | up to 25 foragers or 80 farmers |
| Migration | 10 migrants a month from 950 BCE, arriving in Chikuzen (district 73) |
| Mortality | `LifeSpan.tsv`: life tables from Jomon and Yayoi skeletal remains (CMR 49%) |
| Fertility | TFR 5.5 for farmers and foragers (`Childbearing.tsv`) |
| Marriage | women from 13 (mean 16), men from 17 (mean 18); radius 20 km; bilocal residence |
| Group movement | 0.21% a month, up to 200 km along least-cost paths |
| Agricultural inheritance | 50% for a child of a farmer and a forager |

## Variants

Each variant changes one setting of B (see `Main.tsv`, `E.tsv`, `P.tsv`):

| ID | Change | ID | Change |
| --- | --- | --- | --- |
| C | migration starts 800 BCE | J | group moves 0.14% a month |
| D | 5 migrants a month | K | group moves 0.42% a month |
| E | 20 migrants a month | L | movement range 100 km |
| F | matrilocal residence | M | movement range 400 km |
| G | patrilocal residence | N | agricultural inheritance 25% |
| H | marriage radius 10 km | O | agricultural inheritance 75% |
| I | marriage radius 40 km | P | farmer TFR 5.75 |
|   |   | Q | forager TFR 5.75 |

Expected results for 50 CE are in `Expected.tsv` (1,031,843 people, Crema et al. 2024; 70% continental
ancestry; farming reaching northern Tohoku) and the farming-arrival zones in `Zones.tsv` (Fujio 2009).

## Running

1. Build the runner: `Scripts\BuildBatchSimulator.bat` (Visual Studio 2022; writes `build\BatchSimulator\PAXS_BatchSimulator.exe`).
2. Put a `Config.tsv` in the repository root containing `asset_file` → `./` and run from the root.
3. Run all models (5 runs each, as in the presentation):
   `powershell -File Scripts\SuperviseBatch.ps1` (seed base 20261105, population cap 5,000,000),
   or directly `build\BatchSimulator\PAXS_BatchSimulator.exe batch Data/Simulations/Yayoi-Sim-v2/Batch.tsv --runs 5 --seed-base 20261105 --max-population 5000000 --out SimulationResults/Yayoi-Sim-v2`
   (`Batch.tsv` lists all 16 models; the supervisor runs them in the phases `Main.tsv`, `P.tsv`, `E.tsv`
   to keep the two fastest-growing models from running in parallel).
4. Summaries: `python Scripts/ProfileModels.py SimulationResults/Yayoi-Sim-v2`;
   averaged videos: `python Scripts/ReplayAverages.py SimulationResults/Yayoi-Sim-v2`.

Each run's seed is recorded in `SimulationResults/Yayoi-Sim-v2/<ID>/Seeds.tsv`. The outputs, seeds and videos
used in the presentation are archived on Zenodo (see the presentation for the DOI).
