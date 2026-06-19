#!/usr/bin/env bash
# End to end build. Run from the repository root.
#   1. Python downloads and cleans the price data.
#   2. C++ runs the experiments and writes result CSVs.
#   3. Copy result CSVs into figures/ for pgfplots.
#   4. Build the paper PDF.
set -e

echo "[1/4] Fetch and preprocess price data"
python3 scripts/python/fetch_data.py
python3 scripts/python/preprocess.py

echo "[2/4] Build and run the C++ core"
make run

echo "[3/4] Build figures"
cp results/example_day.csv results/e1_summary.csv results/e2_cdeg_sweep.csv \
   results/e3_granularity.csv results/e4_runtime_vs_T.csv \
   results/e4_runtime_vs_Nsoc.csv figures/
python3 scripts/python/make_figures.py

echo "[4/4] Build the paper"
pdflatex -interaction=nonstopmode -output-directory=paper paper/main.tex
pdflatex -interaction=nonstopmode -output-directory=paper paper/main.tex

echo "Done. See paper/main.pdf"
