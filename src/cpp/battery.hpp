// battery.hpp
// Battery model for the BESS arbitrage study.
//
// State of charge (SoC) is the energy stored in the battery, in MWh.
// An action moves energy into the battery (charge) or out of it (discharge).
// One slot lasts dt hours. Power limits cap how much energy can move in a slot.
//
// Energy convention.
//   charge   : grid energy bought = g. SoC rises by eta_c * g.
//   discharge: SoC falls by h. grid energy sold = eta_d * h.
//   idle     : nothing moves.
// Here g and h are the SoC side energy amounts before efficiency on the grid
// side. We split the round trip efficiency eta into a charge part and a
// discharge part with eta_c = eta_d = sqrt(eta).
//
// Degradation cost. Each unit of energy that flows through the battery wears it
// a little. We use a linear throughput model. The cost of an action is
//   c_deg * (energy charged + energy discharged on the SoC side).
// This is additive and depends only on the current action, so it fits dynamic
// programming with SoC as the only state.

#ifndef BATTERY_HPP
#define BATTERY_HPP

#include <cmath>

struct Battery {
    double e_max;        // usable energy capacity, MWh
    double p_chg_max;    // max charge power, MW
    double p_dis_max;    // max discharge power, MW
    double dt;           // slot length, hours
    double eta_rt;       // round trip efficiency, 0..1
    double c_deg;        // degradation cost, EUR per MWh of SoC side throughput

    // One way efficiency for charge and for discharge.
    double eta_one() const { return std::sqrt(eta_rt); }

    // Max energy that can move on the SoC side in one slot.
    double max_charge_soc() const { return p_chg_max * dt; }
    double max_discharge_soc() const { return p_dis_max * dt; }
};

#endif  // BATTERY_HPP
