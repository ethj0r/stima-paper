# Dynamic Programming for BESS Arbitrage with Degradation Cost

> built IF2211 Strategi Algoritma

## Idea
Schedule one battery to buy cheap and sell expensive electricity over a day.
Each charge and discharge wears the battery, which is a real cost. We add a
linear degradation cost to the dynamic programming reward. This keeps the
problem additive and Markovian, so plain backward induction in
`O(T * N_s * N_a)` time still solves it. On one year of real German day ahead
prices, the degradation aware schedule earns a positive yearly profit, while a
greedy rule and a degradation blind DP both lose money once real wear is
counted.

## Layout
```
data/raw/         downloaded price data
data/processed/   tidy per day price matrix (prices_2019.csv)
src/cpp/          battery model, DP solver, baselines, main driver
scripts/python/   fetch_data.py, preprocess.py
scripts/run_all.sh   end to end build
results/          CSV outputs from the C++ core
figures/          TikZ, pgfplots, and matplotlib figures (from result CSVs)
paper/            LaTeX source (IEEEtran), builds main.pdf
```

## Data source
Day ahead electricity prices from Open Power System Data, time series package,
German and Luxembourg bidding zone, calendar year 2019.
https://data.open-power-system-data.org/time_series/

## Experiments
- E1 main comparison: DP with wear vs DP without wear vs greedy threshold.
- E2 sensitivity to the planned wear cost.
- E3 effect of the SoC grid size on profit and runtime.
- E4 empirical runtime against horizon length and grid size.