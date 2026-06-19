// dp_solver.cpp
#include "dp_solver.hpp"

#include <algorithm>
#include <limits>

namespace {

constexpr double NEG_INF = -std::numeric_limits<double>::infinity();

// Reward for moving from SoC level energy s_e to s_next_e in one slot at price p.
// Positive delta means charging (buy energy), negative means discharging (sell).
// Returns the reward and fills the energy bookkeeping by reference.
double slot_reward(double s_e, double s_next_e, double price,
                   const Battery& b,
                   double& charge_soc, double& discharge_soc,
                   double& buy, double& sell) {
    double delta = s_next_e - s_e;  // change in SoC, MWh
    charge_soc = 0.0;
    discharge_soc = 0.0;
    buy = 0.0;
    sell = 0.0;
    double reward = 0.0;
    if (delta > 0.0) {
        // Charging. SoC rises by delta = eta_one * grid_bought.
        charge_soc = delta;
        buy = delta / b.eta_one();
        reward = -price * buy;
    } else if (delta < 0.0) {
        // Discharging. SoC falls by |delta|. grid_sold = eta_one * |delta|.
        double d = -delta;
        discharge_soc = d;
        sell = b.eta_one() * d;
        reward = price * sell;
    }
    // Degradation on the SoC side throughput of this action.
    reward -= b.c_deg * (charge_soc + discharge_soc);
    return reward;
}

}  // namespace

// Core solver. If value_out is not null it receives the value table. If
// path_out is not null it receives the optimal path of SoC level indices.
static Schedule solve_dp_core(const std::vector<double>& prices,
                              const Battery& batt,
                              int n_soc,
                              double soc0_frac,
                              double socT_min_frac,
                              std::vector<std::vector<double>>* value_out,
                              std::vector<int>* path_out) {
    const int T = static_cast<int>(prices.size());
    const int N = n_soc;
    const double step = batt.e_max / (N - 1);  // MWh per SoC level

    // Energy value of each SoC level.
    auto level_energy = [&](int i) { return i * step; };

    // Power limits as a number of levels reachable per slot.
    const int up_levels =
        static_cast<int>(std::floor(batt.max_charge_soc() / step + 1e-9));
    const int down_levels =
        static_cast<int>(std::floor(batt.max_discharge_soc() / step + 1e-9));

    // Value table V[t][s] and policy table P[t][s] = chosen next level.
    std::vector<std::vector<double>> V(T + 1, std::vector<double>(N, NEG_INF));
    std::vector<std::vector<int>> P(T + 1, std::vector<int>(N, -1));

    // Terminal condition. End SoC must be at least the required minimum.
    const int s_end_min =
        std::max(0, static_cast<int>(std::ceil(socT_min_frac * (N - 1) - 1e-9)));
    for (int s = 0; s < N; ++s) {
        V[T][s] = (s >= s_end_min) ? 0.0 : NEG_INF;
    }

    // Backward induction.
    for (int t = T - 1; t >= 0; --t) {
        const double p = prices[t];
        for (int s = 0; s < N; ++s) {
            const double s_e = level_energy(s);
            int lo = std::max(0, s - down_levels);
            int hi = std::min(N - 1, s + up_levels);
            double best = NEG_INF;
            int best_next = -1;
            for (int s2 = lo; s2 <= hi; ++s2) {
                double v_next = V[t + 1][s2];
                if (v_next == NEG_INF) continue;
                double c, d, b, sell;
                double r = slot_reward(s_e, level_energy(s2), p, batt, c, d, b, sell);
                double val = r + v_next;
                if (val > best) {
                    best = val;
                    best_next = s2;
                }
            }
            V[t][s] = best;
            P[t][s] = best_next;
        }
    }

    // Forward reconstruction from the start SoC.
    Schedule sch;
    sch.soc.assign(T + 1, 0.0);
    sch.charge.assign(T, 0.0);
    sch.discharge.assign(T, 0.0);
    sch.grid_buy.assign(T, 0.0);
    sch.grid_sell.assign(T, 0.0);

    int s0 = static_cast<int>(std::round(soc0_frac * (N - 1)));
    s0 = std::min(std::max(s0, 0), N - 1);
    sch.soc[0] = level_energy(s0);

    std::vector<int> path(T + 1, s0);
    int s = s0;
    for (int t = 0; t < T; ++t) {
        int s2 = P[t][s];
        if (s2 < 0) {
            // No feasible continuation. Stay put (should not happen with a
            // reachable start and a feasible terminal requirement).
            s2 = s;
        }
        double c, d, b, sell;
        slot_reward(level_energy(s), level_energy(s2), prices[t], batt,
                    c, d, b, sell);
        sch.charge[t] = c;
        sch.discharge[t] = d;
        sch.grid_buy[t] = b;
        sch.grid_sell[t] = sell;
        sch.gross_revenue += prices[t] * sell - prices[t] * b;
        sch.deg_cost += batt.c_deg * (c + d);
        sch.throughput += c + d;
        sch.soc[t + 1] = level_energy(s2);
        s = s2;
        path[t + 1] = s2;
    }
    sch.net_profit = sch.gross_revenue - sch.deg_cost;
    sch.full_cycles = sch.throughput / (2.0 * batt.e_max);

    if (value_out) *value_out = V;
    if (path_out) *path_out = path;
    return sch;
}

Schedule solve_dp(const std::vector<double>& prices,
                  const Battery& batt,
                  int n_soc,
                  double soc0_frac,
                  double socT_min_frac) {
    return solve_dp_core(prices, batt, n_soc, soc0_frac, socT_min_frac,
                         nullptr, nullptr);
}

Schedule solve_dp_with_table(const std::vector<double>& prices,
                             const Battery& batt,
                             int n_soc,
                             double soc0_frac,
                             double socT_min_frac,
                             std::vector<std::vector<double>>& value_table,
                             std::vector<int>& path) {
    return solve_dp_core(prices, batt, n_soc, soc0_frac, socT_min_frac,
                         &value_table, &path);
}
