"""Preprocess raw OPSD prices into a tidy per-day matrix for the C++ solver.

Input : data/raw/opsd_de_lu_price.csv  (timestamp, price) hourly, EUR/MWh.
Output: data/processed/prices_2019.csv

Output format. One row per day. The first column is the date. The next 24
columns are the hourly day-ahead prices for that day, named h00 .. h23 in local
clock order. We keep only full days with all 24 hours present. We restrict to
the calendar year 2019, which is a clean full year of German day-ahead prices.

The C++ DP solver reads this file. Each day is one independent scheduling
horizon with T = 24 slots and dt = 1 hour.
"""

import os
import sys
import pandas as pd

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RAW = os.path.join(ROOT, "data", "raw", "opsd_de_lu_price.csv")
OUT_DIR = os.path.join(ROOT, "data", "processed")
OUT = os.path.join(OUT_DIR, "prices_2019.csv")

YEAR = 2019


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    df = pd.read_csv(RAW, parse_dates=["timestamp"])

    # Work in a fixed local clock so each day has a clean 00..23 hour index.
    # OPSD timestamps are UTC. We shift to Central European Time (UTC+1) as a
    # fixed offset. We do not model daylight saving here, a fixed offset is
    # enough for a per day arbitrage study and keeps every day at 24 slots.
    df["local"] = df["timestamp"] + pd.Timedelta(hours=1)
    df["date"] = df["local"].dt.date
    df["hour"] = df["local"].dt.hour

    df = df[df["local"].dt.year == YEAR]

    # Pivot to one row per day, 24 hour columns.
    wide = df.pivot_table(index="date", columns="hour", values="price")
    # Keep only days that have all 24 hours.
    wide = wide.dropna(axis=0, how="any")
    wide = wide[[h for h in range(24)]]
    wide.columns = [f"h{h:02d}" for h in range(24)]
    wide = wide.sort_index()

    wide.to_csv(OUT, index_label="date")
    print(f"Wrote {len(wide)} full days to {OUT}")
    flat = wide.to_numpy().ravel()
    print(f"Hourly price EUR/MWh  min {flat.min():.2f}  "
          f"mean {flat.mean():.2f}  max {flat.max():.2f}")
    # Average daily spread, a quick sanity check on arbitrage room.
    spread = (wide.max(axis=1) - wide.min(axis=1)).mean()
    print(f"Mean daily max-min spread: {spread:.2f} EUR/MWh")


if __name__ == "__main__":
    sys.exit(main())
