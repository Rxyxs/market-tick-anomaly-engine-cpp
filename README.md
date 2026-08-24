[ 🇺🇸 English ] | [ 🇨🇱 Leer en Español ](README.es.md)

# 1. Project Title

## Market Tick Anomaly Engine — Zero-Dependency Real-Time Detection in C++

![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?style=flat&logo=cplusplus&logoColor=white)
![MSVC](https://img.shields.io/badge/compiler-MSVC%20(cl.exe)-5C2D91?style=flat)
![Dependencies](https://img.shields.io/badge/external%20dependencies-zero-brightgreen?style=flat)
![Tests](https://img.shields.io/badge/tests-24%2F24%20passing-brightgreen?style=flat)
![Data](https://img.shields.io/badge/data-real%20Binance%20trades-lightgrey?style=flat)
![Status](https://img.shields.io/badge/status-validated%20against%20a%20real%20event-blue?style=flat)

A streaming anomaly-detection engine for financial tick data, written in
pure **C++17 with zero external dependencies** — no Boost, no vcpkg
package, not even a plotting library: the engine parses its own CSV, runs
its own online statistics, and writes its own SVG charts. It processes
**4.78 million real trades** (not simulated) from Binance's public
historical archive, covering **March 11–13, 2020 — crypto's "Black
Thursday,"** when BTC fell roughly 50% in 24 hours. Four hand-rolled
streaming detectors (EWMA z-score, CUSUM, volume-burst ratio, order-flow
imbalance) run as one ensemble, entirely in a single pass, at **2.7 million
trades/second**.

> This is the third project in a small portfolio arc on financial anomaly
> detection — see
> [chile-aml-anomaly-detection-engine](https://github.com/Rxyxs/chile-aml-anomaly-detection-engine)
> (unsupervised graph ensemble, synthetic AML data, Python) and
> [credit-fraud-autoencoder-detection-engine](https://github.com/Rxyxs/credit-fraud-autoencoder-detection-engine)
> (deep autoencoder vs. supervised XGBoost, real labeled fraud data,
> Python). This one deliberately changes two things at once: the language
> (C++, closing a gap my other repos only approximated with C# and a C hot
> path) and the validation problem itself — **real market data has no
> fraud-style ground-truth label**, so this project has to validate its
> detector a different way: against a real, independently documented
> market event, not a confusion matrix.

---

# 2. Motivation

Two separate problems shaped this project on purpose:

**Why C++, and why zero dependencies.** My public bio lists Python, SQL,
R, and C++. Two of those had a real repo before today; C++ so far only had
an adjacent stand-in (a C# desktop app, and a C-compiled scoring DLL called
from Python). This project is a literal, unhedged C++ deliverable: the
entire pipeline — CSV parsing, streaming statistics, ensemble scoring, and
even the SVG chart rendering — is native C++17 compiled with MSVC, with no
package manager and no external library. That constraint is deliberate,
not a limitation worked around: real-time market-data infrastructure is
written this way precisely because every dependency is a latency and
operational-risk line item.

**Why market microstructure, not another transaction dataset.** My other
two anomaly-detection projects both work at the level of an *account* or a
*transaction* with a knowable fraud label attached. Real exchange trade
data has neither — a trade is just `(price, quantity, time, aggressor
side)`, with no "this was manipulation" flag anywhere. That forces a
genuinely different detection problem (streaming statistical process
control instead of a feature-matrix classifier) and a genuinely different
validation strategy (§5), which is the point: three projects that each
answer "how do you detect financial anomalies" under a different real-world
constraint, not the same solution copy-pasted three times.

---

# 3. Theoretical Framework

Four independent, complementary online detectors run over 1-minute OHLCV
bars aggregated from raw trades:

| Detector | Mechanism | Catches |
|---|---|---|
| **EWMA z-score** | Exponentially-weighted moving mean/variance of the log-return; z-score computed against the *pre-update* state so a shock can't dilute the baseline that's judging it. | A single violent price move in one bar. |
| **CUSUM** (Page, 1954) | Tabular cumulative sum of the standardized return (the EWMA z-score above), separately accumulated in each direction. | A *sustained* drift too gradual for any one bar's z-score to flag, but persistent over several minutes. |
| **Volume-burst ratio** | Trade count in the current bar over the trailing rolling mean. | A sudden surge in trading activity, independent of price direction. |
| **Order-flow imbalance z-score** | EWMA z-score of `(taker_buy_volume − taker_sell_volume) / total_volume` per bar — the standard "aggressor side" classification from `is_buyer_maker`. | One-sided, aggressive selling (or buying) pressure — the microstructure signature of a real liquidation cascade, distinct from a price move alone. |

Each detector emits a component normalized so that `1.0` sits at its own
decision threshold; the **ensemble score is the max of the four** (any one
strong mechanism fires the alert — the same "maximization" combination
philosophy used in the PyOD ensemble of the sibling AML project), with the
average also reported per bar for context.

**A 30-bar warm-up is mandatory, not optional.** A streaming z-score
computed against a variance estimate of exactly zero (which is what you get
after a *single* observation) divides by a near-zero floor and produces a
meaningless, arbitrarily large value. Early iterations of this engine hit
exactly that: the second bar producing a z-score of −184,855 and
permanently saturating CUSUM for the entire 3-day run (213 real alerts
became 4,319 — essentially everything). The fix is a proper 30-bar
warm-up that estimates an initial mean/variance from a simple average
before switching to the exponential update — documented here because it's
a real, general lesson for any streaming detector, not specific to this
dataset.

---

# 4. Explanation

## Pipeline architecture

```mermaid
flowchart LR
    A["download_data.ps1<br/>real BTC/USDT trades (Binance)<br/>11-13 mar 2020, no auth needed"] --> B["csv_reader.cpp<br/>manual streaming CSV parser<br/>2.7M trades/sec"]
    B --> C["bar_aggregator.cpp<br/>1-min OHLCV + taker buy/sell<br/>volume, gap-filled"]
    C --> D["detectors/ensemble.cpp<br/>EWMA z-score + CUSUM +<br/>volume burst + order-flow imbalance"]
    D --> E["main.cpp<br/>bars_with_signals.csv +<br/>lift vs. documented crash window"]
    D --> F["svg_writer.cpp<br/>self-drawn SVG charts,<br/>zero plotting libraries"]
```

## Module responsibilities

| Module | Responsibility |
|---|---|
| [`data/download_data.ps1`](data/download_data.ps1) | Fetches the real trade data from Binance's public archive (no API key needed) and unzips it. |
| [`src/csv_reader.cpp`](src/csv_reader.cpp) | Hand-written streaming CSV parser (no `std::stringstream`/`getline` in the hot path) for the fixed 7-column trade format. |
| [`src/bar_aggregator.cpp`](src/bar_aggregator.cpp) | Aggregates trades into 1-minute OHLCV bars with taker buy/sell volume split; fills any trade-less minute with a flat bar so detectors see a uniform cadence. |
| [`src/detectors/ewma_zscore.*`](src/detectors/ewma_zscore.hpp) | Streaming z-score with warm-up seeding (§3). |
| [`src/detectors/cusum.hpp`](src/detectors/cusum.hpp) | Tabular CUSUM for sustained drift. |
| [`src/detectors/rolling_ratio.hpp`](src/detectors/rolling_ratio.hpp) | Trailing-window ratio for the volume-burst detector. |
| [`src/detectors/ensemble.*`](src/detectors/ensemble.hpp) | Wires the four detectors together per bar into `AnomalySignal`. |
| [`src/svg_writer.*`](src/svg_writer.hpp) | Self-contained SVG line-chart renderer — no external graphics dependency. |
| [`src/main.cpp`](src/main.cpp) | CLI orchestrator: parses, aggregates, detects, evaluates against the documented crash window, writes CSV + SVG. |
| [`tests/test_main.cpp`](tests/test_main.cpp) | Hand-rolled assertion-based test suite (no external test framework — this machine has no C++ package manager configured). |

---

# 5. Methodology

- **No lookahead, anywhere.** Every detector updates its internal state
  strictly from past and current bars; the z-score for bar *i* is computed
  against the mean/variance estimated *before* bar *i* is folded in.
- **Validation against a real, independently documented event, not a
  confusion matrix.** Unlike the sibling AML/fraud projects, there is no
  `is_anomaly` column in real exchange data. This project instead asks: do
  the detector's strongest signals land where a real, widely-documented
  market crash actually happened? March 12–13, 2020 is used as the
  comparison window precisely because it needs no fabricated label — it's
  public financial history.
- **Two honest engineering findings surfaced and fixed, not smoothed
  over**:
  1. The cold-start z-score bug described in §3, found by noticing the
     alert rate was numerically impossible (99.98% of all bars flagged)
     rather than by assuming the design was correct.
  2. The pipeline's latency bottleneck isn't the detection logic — it's
     single-threaded I/O reading three multi-hundred-megabyte CSVs: the
     detection loop over 4,320 bars is unmeasurably fast (0 ms at
     millisecond resolution), while parsing 4.78M lines takes ~1.8s of the
     ~1.89s total — a real, worked illustration of where the time actually
     goes in a systems-level pipeline, not where intuition suggests it
     should.
- **The `outputs/reports/*.csv` and `outputs/bin/*.exe` are gitignored and
  regenerated by `build.ps1` + `market_anomaly_engine.exe`**; only the
  small SVG figures (needed for this README) are version-controlled,
  following the same artifact-hygiene pattern as the rest of this
  portfolio.

---

# 6. Development

## Requirements

- Windows with **Visual Studio 2019/2022** (Community or Build Tools),
  workload "Desktop development with C++" — `cl.exe` is invoked directly,
  no CMake or other build system is required.
- PowerShell (for the data-download script and the build script).

## Download the real dataset

```powershell
powershell -File data\download_data.ps1
```

Fetches `BTCUSDT-trades-2020-03-11/12/13.csv` (~345 MB uncompressed)
directly from Binance's public data archive — no account or API key
needed.

## Build

```powershell
powershell -File build.ps1
```

Compiles `outputs\bin\market_anomaly_engine.exe` (the engine) and
`outputs\bin\run_tests.exe` (the test suite) via `cl.exe`.

## Run

```powershell
.\outputs\bin\market_anomaly_engine.exe data\raw\BTCUSDT-trades-2020-03-11.csv data\raw\BTCUSDT-trades-2020-03-12.csv data\raw\BTCUSDT-trades-2020-03-13.csv
```

## Tests

```powershell
.\outputs\bin\run_tests.exe
```

## Project structure

```
market-tick-anomaly-engine-cpp/
├── data/
│   ├── download_data.ps1        # fetches real Binance trade data
│   └── raw/                     # BTCUSDT-trades-*.csv (real, ~345 MB, gitignored)
├── src/
│   ├── trade_types.hpp/.cpp     # Trade, Bar
│   ├── csv_reader.hpp/.cpp      # streaming CSV parser
│   ├── bar_aggregator.hpp/.cpp  # tick -> 1-min OHLCV bars
│   ├── detectors/
│   │   ├── ewma_zscore.hpp/.cpp
│   │   ├── cusum.hpp
│   │   ├── rolling_ratio.hpp
│   │   └── ensemble.hpp/.cpp
│   ├── svg_writer.hpp/.cpp      # self-contained SVG chart renderer
│   └── main.cpp                 # CLI orchestrator
├── tests/
│   └── test_main.cpp            # hand-rolled assertion test suite
├── outputs/
│   ├── bin/                     # compiled .exe (gitignored)
│   ├── reports/                 # bars_with_signals.csv (gitignored)
│   └── figures/                 # SVG charts (version-controlled)
├── build.ps1
├── README.md
└── README.es.md
```

---

# 7. Results

Every number below comes from an actual run of `market_anomaly_engine.exe`
against the real, complete 3-day dataset.

## 7.1 Dataset and performance

| Metric | Value |
|---|---|
| Real trades processed | 4,780,043 (699,401 + 1,770,812 + 2,309,830 across the 3 days) |
| 1-minute bars produced | 4,320 |
| CSV parse throughput | ~2.7 million trades/second |
| Total pipeline time (parse + aggregate + detect + write CSV/SVG) | 1.89 seconds |

## 7.2 Headline validation: the single most anomalous minute in the dataset

The highest ensemble score across all 4,320 minutes — **5.55**, nearly
double the runner-up day's peak of 3.04 — occurs at **2020-03-12 10:47
UTC**, exactly during a real, sudden collapse visible directly in the raw
data: price fell from **$6,447 at 10:44 to $5,600 at 10:47** (roughly
−13% in three minutes) before partially recovering. This is not a
cherry-picked window — it's simply the single highest-scoring bar the
ensemble produced, and it lands inside the real, independently documented
crash.

![Price and alerts](outputs/figures/price_and_alerts.svg)

![Ensemble score over time](outputs/figures/ensemble_score.svg)

## 7.3 Alert enrichment by day

| Day | Alerts | Bars | Alert rate |
|---|---:|---:|---:|
| Mar 11 (calm baseline) | 51 | 1,440 | 3.54% |
| **Mar 12 (crash day)** | **107** | **1,440** | **7.43%** |
| Mar 13 (continued volatility) | 55 | 1,440 | 3.82% |

March 12's alert rate is **~2.0x** the average of the two surrounding
days — a real, non-trivial enrichment, and an honest one: it is not a
dramatic 10x separation, because even a "calm" day in a liquid crypto pair
has genuine short-lived volume and price bursts of its own. Over the full
48-hour documented crash window (Mar 12–13 combined against Mar 11 as
baseline), 162 of 213 alerts (76.1%) fall inside it, against a 66.7% base
rate — a modest but real 1.14x lift, diluted by the fact that the window
itself spans a full 48 hours while the sharpest moves (§7.2) last minutes.

## 7.4 A bug the numbers themselves exposed

The first working version of this engine flagged 4,319 of 4,320 bars as
anomalous — a number that's numerically impossible to take at face value
for a detector meant to be selective. Tracing it down (§3) found a genuine
cold-start bug in the EWMA variance estimator, not a calibration choice;
fixing it dropped the alert count to a believable 213 (4.9% of bars). This
is kept in the report deliberately: an implausible summary statistic was
the signal that something was wrong, not a symptom to explain away.

---

# 8. Conclusion

- **A zero-dependency C++ engine processes 4.78M real trades and 4,320
  bars of streaming detection in under 2 seconds**, closing a literal gap
  in my public skill claims that two prior repos only approximated with
  C# and a C hot path.
- **The ensemble's single strongest signal lands exactly inside a real,
  independently documented market event** (§7.2) — the most rigorous form
  of validation available when, unlike the sibling fraud/AML projects, no
  ground-truth label exists at all for real market data.
- **The day-level enrichment is real but modest (~2x on the crash day)**,
  reported honestly rather than inflated — liquid markets are noisy even
  on ordinary days, and a detector tuned to fire only on the single most
  extreme event in 4,320 minutes would have far lower recall on the
  broader crash period.
- **A cold-start numerical bug was caught by an implausible result, not by
  code review** — a reminder that "nearly everything got flagged" is a
  finding to investigate, never a threshold to quietly raise.

## Future work

- Extend the detector to full limit-order-book data (not just executed
  trades) where available, to catch spoofing/layering patterns that trade
  prints alone cannot reveal.
- Add a lock-free ring buffer and a live WebSocket feed handler so the
  engine can score a live market stream instead of a historical file — the
  natural next step for something already architected as O(1)-per-bar
  streaming state.
- Cross-validate against a second, independent real crash event (e.g. May
  19, 2021) to check whether the ~2x day-level enrichment found here
  generalizes or was specific to Black Thursday's particular dynamics.
- Feed the ensemble's continuous score (not just the binary alert) into a
  downstream supervised model once enough analyst-confirmed labels exist —
  the same unsupervised-to-supervised transition quantified in
  [credit-fraud-autoencoder-detection-engine](https://github.com/Rxyxs/credit-fraud-autoencoder-detection-engine).

---

# 9. Author

**Pablo Reyes** — [github.com/Rxyxs](https://github.com/Rxyxs)

## Data source & license

Trade data: real, public historical trades for BTC/USDT on Binance,
March 11–13, 2020, downloaded from
[Binance's public data archive](https://data.binance.vision) (no
authentication required, no rate limits for historical daily files). This
project performs no trading, order placement, or account access — it reads
already-published historical trade prints for offline analysis.

Code: MIT — see [LICENSE](LICENSE).
