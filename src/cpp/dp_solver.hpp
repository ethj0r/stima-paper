// dp_solver.hpp
// Dynamic programming solver for single battery energy arbitrage.
//
// Problem. Given hourly prices p[0..T-1] for one day and a battery, choose a
// charge or discharge action in each slot to maximize net profit. Net profit is
// revenue from energy sold minus cost of energy bought minus degradation cost.
//
// DP formulation.
//   Stage  t = 0..T. State s = SoC level index in 0..N_s-1.
//   We discretize SoC into N_s levels on [0, E_max].
//   The decision in slot t is the next SoC level. The change in SoC fixes the
//   charge or discharge energy, so power limits restrict the reachable next
//   levels. This keeps every transition exactly on the grid.
//   Reward R_t(s, s') = price effect of the energy moved minus degradation.
//   Bellman: V_t(s) = max over feasible s' [ R_t(s, s') + V_{t+1}(s') ].
//   Solved by backward induction. We store the argmax to rebuild the schedule.
//
// Complexity. O(T * N_s * N_a) where N_a is the number of reachable next levels
// per state, bounded by the power limit window.

#ifndef DP_SOLVER_HPP
#define DP_SOLVER_HPP

#include <vector>
#include "battery.hpp"

// Result of scheduling one day.
struct Schedule {
    std::vector<double> soc;        // SoC at each stage, length T+1, MWh
    std::vector<double> charge;     // SoC side energy charged per slot, length T
    std::vector<double> discharge;  // SoC side energy discharged per slot, length T
    std::vector<double> grid_buy;   // grid energy bought per slot, MWh
    std::vector<double> grid_sell;  // grid energy sold per slot, MWh
    double net_profit = 0.0;        // EUR, includes degradation cost
    double gross_revenue = 0.0;     // EUR sold minus EUR bought, no degradation
    double deg_cost = 0.0;          // EUR degradation
    double throughput = 0.0;        // total SoC side energy moved, MWh
    double full_cycles = 0.0;       // equivalent full cycles = throughput / (2 e_max)
};

// Solve one day with dynamic programming.
//   prices : length T hourly prices, EUR/MWh.
//   batt   : battery parameters. Set batt.c_deg = 0 for the no degradation variant.
//   n_soc  : number of SoC levels (grid resolution).
//   soc0_frac, socT_min_frac : start SoC and required end SoC as fractions of e_max.
// Terminal value is zero. We require the end SoC to be at least socT_min_frac so
// the battery is not simply drained for free at the end of the day.
Schedule solve_dp(const std::vector<double>& prices,
                  const Battery& batt,
                  int n_soc,
                  double soc0_frac,
                  double socT_min_frac);

// Same as solve_dp, but also fills the value table V and the path of SoC level
// indices that the optimal schedule follows. Used to draw the DP table heatmap.
// value_table has shape (T+1) by n_soc. Infeasible cells are left as a large
// negative number. path has length T+1 with the SoC level index at each stage.
Schedule solve_dp_with_table(const std::vector<double>& prices,
                             const Battery& batt,
                             int n_soc,
                             double soc0_frac,
                             double socT_min_frac,
                             std::vector<std::vector<double>>& value_table,
                             std::vector<int>& path);

#endif  // DP_SOLVER_HPP
