// baselines.cpp
#include "baselines.hpp"

#include <algorithm>

namespace {

// Linear interpolation quantile of a price vector.
double quantile(std::vector<double> v, double q) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    if (q <= 0.0) return v.front();
    if (q >= 1.0) return v.back();
    double pos = q * (v.size() - 1);
    size_t i = static_cast<size_t>(pos);
    double frac = pos - i;
    if (i + 1 < v.size()) return v[i] * (1.0 - frac) + v[i + 1] * frac;
    return v[i];
}

}  // namespace

Schedule solve_greedy(const std::vector<double>& prices,
                      const Battery& batt,
                      double q_low,
                      double q_high,
                      double soc0_frac) {
    const int T = static_cast<int>(prices.size());
    const double thr_low = quantile(prices, q_low);
    const double thr_high = quantile(prices, q_high);

    Schedule sch;
    sch.soc.assign(T + 1, 0.0);
    sch.charge.assign(T, 0.0);
    sch.discharge.assign(T, 0.0);
    sch.grid_buy.assign(T, 0.0);
    sch.grid_sell.assign(T, 0.0);

    double soc = soc0_frac * batt.e_max;
    sch.soc[0] = soc;
    const double eta = batt.eta_one();

    for (int t = 0; t < T; ++t) {
        double p = prices[t];
        double c = 0.0, d = 0.0, buy = 0.0, sell = 0.0;
        if (p <= thr_low) {
            // Charge as much as power limit and free capacity allow.
            double room = batt.e_max - soc;
            c = std::min(batt.max_charge_soc(), room);
            c = std::max(c, 0.0);
            buy = c / eta;
            soc += c;
        } else if (p >= thr_high) {
            // Discharge as much as power limit and available energy allow.
            d = std::min(batt.max_discharge_soc(), soc);
            d = std::max(d, 0.0);
            sell = eta * d;
            soc -= d;
        }
        sch.charge[t] = c;
        sch.discharge[t] = d;
        sch.grid_buy[t] = buy;
        sch.grid_sell[t] = sell;
        sch.gross_revenue += p * sell - p * buy;
        sch.deg_cost += batt.c_deg * (c + d);
        sch.throughput += c + d;
        sch.soc[t + 1] = soc;
    }
    sch.net_profit = sch.gross_revenue - sch.deg_cost;
    sch.full_cycles = sch.throughput / (2.0 * batt.e_max);
    return sch;
}
