"""Fetch real hourly electricity price data.

Source: Open Power System Data (OPSD), time series package.
URL: https://data.open-power-system-data.org/time_series/

We use the day-ahead price column for the German/Luxembourg bidding zone
(DE_LU_price_day_ahead). These are real day-ahead market clearing prices in
EUR/MWh at hourly resolution. OPSD is an open, citable dataset widely used in
electricity price forecasting research.

The script downloads the single-index 60 minute CSV, keeps the timestamp and the
chosen price column, and saves a small raw CSV into data/raw. Preprocessing into
the tidy per-day format happens in preprocess.py.
"""

import os
import sys
import io
import requests
import pandas as pd

OPSD_URL = (
    "https://data.open-power-system-data.org/time_series/"
    "2020-10-06/time_series_60min_singleindex.csv"
)
PRICE_COL = "DE_LU_price_day_ahead"
TIME_COL = "utc_timestamp"

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
RAW_DIR = os.path.join(ROOT, "data", "raw")
RAW_OUT = os.path.join(RAW_DIR, "opsd_de_lu_price.csv")


def main():
    os.makedirs(RAW_DIR, exist_ok=True)
    print(f"Downloading OPSD time series from:\n  {OPSD_URL}")
    # Stream and parse only the two columns we need, to keep memory low.
    resp = requests.get(OPSD_URL, timeout=180)
    resp.raise_for_status()
    print(f"Downloaded {len(resp.content)/1e6:.1f} MB")

    df = pd.read_csv(
        io.BytesIO(resp.content),
        usecols=[TIME_COL, PRICE_COL],
        parse_dates=[TIME_COL],
    )
    df = df.rename(columns={TIME_COL: "timestamp", PRICE_COL: "price"})
    df = df.dropna(subset=["price"]).reset_index(drop=True)
    df.to_csv(RAW_OUT, index=False)
    print(f"Saved {len(df)} hourly rows to {RAW_OUT}")
    print(f"Date range: {df['timestamp'].min()}  ->  {df['timestamp'].max()}")
    print(f"Price EUR/MWh  min {df['price'].min():.2f}  "
          f"mean {df['price'].mean():.2f}  max {df['price'].max():.2f}")


if __name__ == "__main__":
    sys.exit(main())
