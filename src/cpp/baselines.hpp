// baselines.hpp
// Baseline scheduling methods for comparison against dynamic programming.
//
// Greedy threshold. A myopic rule with no foresight. It looks at the price in
// the current slot only. If the price is below a low threshold it charges as
// much as the power limit and free capacity allow. If the price is above a high
// threshold it discharges as much as allowed. Otherwise it stays idle. The
// thresholds come from price quantiles of the same day. The greedy rule ignores
// degradation when it decides, which is the point of the comparison.
//
// The DP without degradation baseline is not here. It is just solve_dp with the
// battery degradation cost set to zero, so we reuse the DP solver for that.

#ifndef BASELINES_HPP
#define BASELINES_HPP

#include <vector>
#include "battery.hpp"
#include "dp_solver.hpp"

// Greedy threshold schedule for one day.
//   prices  : length T hourly prices.
//   batt    : battery parameters. Degradation cost is still charged in the
//             reported profit, even though the rule does not use it to decide.
//   q_low, q_high : price quantiles in 0..1 for the buy and sell thresholds.
//   soc0_frac : start SoC as a fraction of e_max.
// The reported net profit subtracts the same degradation cost model, so the
// comparison with DP is fair.
Schedule solve_greedy(const std::vector<double>& prices,
                      const Battery& batt,
                      double q_low,
                      double q_high,
                      double soc0_frac);

#endif  // BASELINES_HPP
