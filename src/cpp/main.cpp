// main.cpp
// Driver for the BESS arbitrage experiments E1 to E4.
//
// Reads data/processed/prices_2019.csv. Each row is one day with 24 hourly
// prices. Runs three methods on every day:
//   - DP with degradation cost      (the proposed method)
//   - DP without degradation cost   (c_deg = 0)
//   - Greedy threshold              (myopic baseline)
// Writes result CSVs into results/.
//
// Battery parameters are normalized to a 1 MWh battery. They follow common
// values from the literature, see the paper for sources.

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "battery.hpp"
#include "baselines.hpp"
#include "dp_solver.hpp"

struct Day {
    std::string date;
    std::vector<double> prices;  // length 24
};

static std::vector<Day> read_prices(const std::string& path) {
    std::vector<Day> days;
    std::ifstream in(path);
    if (!in) {
        std::cerr << "Cannot open " << path << "\n";
        return days;
    }
    std::string line;
    std::getline(in, line);  // header
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::stringstream ss(line);
        std::string cell;
        Day d;
        std::getline(ss, cell, ',');
        d.date = cell;
        while (std::getline(ss, cell, ',')) {
            d.prices.push_back(std::stod(cell));
        }
        if (d.prices.size() == 24) days.push_back(std::move(d));
    }
    return days;
}

// Base battery. A 1 MWh battery, 0.5 C rate, 0.86 round trip efficiency.
static Battery base_battery() {
    Battery b;
    b.e_max = 1.0;       // MWh
    b.p_chg_max = 0.5;   // MW, gives 0.5 MWh per 1 h slot
    b.p_dis_max = 0.5;   // MW
    b.dt = 1.0;          // hours
    b.eta_rt = 0.86;     // round trip efficiency
    b.c_deg = 12.0;      // EUR per MWh SoC side throughput, see paper
    return b;
}

static const int N_SOC = 101;      // SoC levels for the main runs
static const double SOC0 = 0.5;    // start at half full
static const double SOCT_MIN = 0.5;  // end at least half full
static const double Q_LOW = 0.30;  // greedy buy quantile
static const double Q_HIGH = 0.70; // greedy sell quantile

// E1. Main comparison over all days. One summary row per day per method, plus
// an aggregate line printed to stdout.
static void run_e1(const std::vector<Day>& days) {
    Battery bdeg = base_battery();
    Battery bno = base_battery();
    bno.c_deg = 0.0;

    // The real degradation cost used to score every schedule fairly. Each method
    // chooses its actions in its own way, but in the real world every unit of
    // throughput wears the battery. So we always score net profit with this same
    // real cost, even for the method that ignores degradation when it decides.
    const double real_c_deg = bdeg.c_deg;

    std::ofstream out("results/e1_daily.csv");
    out << "date,method,true_net_profit,gross_revenue,real_deg_cost,throughput,"
           "full_cycles\n";

    double sum_net[3] = {0, 0, 0};
    double sum_gross[3] = {0, 0, 0};
    double sum_deg[3] = {0, 0, 0};
    double sum_thr[3] = {0, 0, 0};
    double sum_cyc[3] = {0, 0, 0};

    auto emit = [&](const std::string& date, const char* m, const Schedule& s,
                    int idx) {
        double real_deg = real_c_deg * s.throughput;
        double true_net = s.gross_revenue - real_deg;
        out << date << "," << m << "," << true_net << "," << s.gross_revenue
            << "," << real_deg << "," << s.throughput << "," << s.full_cycles
            << "\n";
        sum_net[idx] += true_net;
        sum_gross[idx] += s.gross_revenue;
        sum_deg[idx] += real_deg;
        sum_thr[idx] += s.throughput;
        sum_cyc[idx] += s.full_cycles;
    };

    for (const auto& d : days) {
        Schedule dp = solve_dp(d.prices, bdeg, N_SOC, SOC0, SOCT_MIN);
        Schedule dp0 = solve_dp(d.prices, bno, N_SOC, SOC0, SOCT_MIN);
        Schedule gr = solve_greedy(d.prices, bdeg, Q_LOW, Q_HIGH, SOC0);
        emit(d.date, "dp_deg", dp, 0);
        emit(d.date, "dp_nodeg", dp0, 1);
        emit(d.date, "greedy", gr, 2);
    }

    std::ofstream agg("results/e1_summary.csv");
    agg << "method,total_true_net,total_gross,total_real_deg,total_throughput,"
           "total_cycles\n";
    const char* names[3] = {"dp_deg", "dp_nodeg", "greedy"};
    for (int i = 0; i < 3; ++i) {
        agg << names[i] << "," << sum_net[i] << "," << sum_gross[i] << ","
            << sum_deg[i] << "," << sum_thr[i] << "," << sum_cyc[i] << "\n";
    }
    std::cout << "E1 done over " << days.size() << " days\n";
    for (int i = 0; i < 3; ++i) {
        std::cout << "  " << names[i] << "  true_net " << sum_net[i] << "  gross "
                  << sum_gross[i] << "  real_deg " << sum_deg[i] << "  cycles "
                  << sum_cyc[i] << "\n";
    }
}

// E2. Sensitivity to the degradation cost c_deg. Sweep c_deg, report aggregate
// net profit, gross revenue, throughput and cycles for the DP method.
static void run_e2(const std::vector<Day>& days) {
    // The real operating degradation cost is fixed. We vary only the value the
    // DP plans with. A planning cost above the real cost makes the schedule too
    // shy. A planning cost equal to the real cost is the right choice. The true
    // net column is always scored at the real cost.
    const double real_c_deg = base_battery().c_deg;
    std::ofstream out("results/e2_cdeg_sweep.csv");
    out << "plan_c_deg,true_net,total_gross,total_real_deg,total_throughput,"
           "total_cycles\n";
    std::vector<double> cdegs = {0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 25, 30};
    for (double cd : cdegs) {
        Battery b = base_battery();
        b.c_deg = cd;
        double gross = 0, thr = 0, cyc = 0;
        for (const auto& d : days) {
            Schedule s = solve_dp(d.prices, b, N_SOC, SOC0, SOCT_MIN);
            gross += s.gross_revenue;
            thr += s.throughput;
            cyc += s.full_cycles;
        }
        double real_deg = real_c_deg * thr;
        double true_net = gross - real_deg;
        out << cd << "," << true_net << "," << gross << "," << real_deg << ","
            << thr << "," << cyc << "\n";
    }
    std::cout << "E2 done\n";
}

// E3. Discretization granularity. Vary N_soc, report net profit and mean runtime
// per day. Optimality should rise then flatten as the grid gets finer.
static void run_e3(const std::vector<Day>& days) {
    std::ofstream out("results/e3_granularity.csv");
    out << "n_soc,total_net,mean_ms_per_day\n";
    std::vector<int> grids = {11, 21, 41, 51, 101, 201, 401};
    Battery b = base_battery();
    for (int n : grids) {
        double net = 0;
        auto t0 = std::chrono::high_resolution_clock::now();
        for (const auto& d : days) {
            Schedule s = solve_dp(d.prices, b, n, SOC0, SOCT_MIN);
            net += s.net_profit;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        out << n << "," << net << "," << (ms / days.size()) << "\n";
        std::cout << "E3 n_soc " << n << " net " << net << " ms/day "
                  << (ms / days.size()) << "\n";
    }
    std::cout << "E3 done\n";
}

// E4. Empirical runtime against horizon T and against N_soc, to confirm the
// O(T * N_s * N_a) cost. We stitch days together to build longer horizons and
// time a single solve.
static void run_e4(const std::vector<Day>& days) {
    // Build one long price series by concatenating days.
    std::vector<double> all;
    for (const auto& d : days)
        for (double p : d.prices) all.push_back(p);

    Battery b = base_battery();

    // Runtime against T at fixed N_soc.
    {
        std::ofstream out("results/e4_runtime_vs_T.csv");
        out << "T,n_soc,ms\n";
        int n = 101;
        std::vector<int> Ts = {24, 48, 96, 168, 336, 720, 1440, 2880};
        for (int T : Ts) {
            if (T > (int)all.size()) break;
            std::vector<double> seg(all.begin(), all.begin() + T);
            // Repeat a few times for a stable measurement on small T.
            int reps = std::max(1, 2000 / T);
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int r = 0; r < reps; ++r)
                solve_dp(seg, b, n, SOC0, 0.0);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms =
                std::chrono::duration<double, std::milli>(t1 - t0).count() / reps;
            out << T << "," << n << "," << ms << "\n";
        }
    }

    // Runtime against N_soc at fixed T.
    {
        std::ofstream out("results/e4_runtime_vs_Nsoc.csv");
        out << "T,n_soc,ms\n";
        int T = 168;
        std::vector<double> seg(all.begin(), all.begin() + T);
        std::vector<int> ns = {26, 51, 101, 201, 401, 801, 1601};
        for (int n : ns) {
            int reps = std::max(1, 400000 / (T * n));
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int r = 0; r < reps; ++r)
                solve_dp(seg, b, n, SOC0, 0.0);
            auto t1 = std::chrono::high_resolution_clock::now();
            double ms =
                std::chrono::duration<double, std::milli>(t1 - t0).count() / reps;
            out << T << "," << n << "," << ms << "\n";
        }
    }
    std::cout << "E4 done\n";
}

// Export one example day schedule for the figure: price, SoC, charge, discharge.
static void export_example_day(const std::vector<Day>& days) {
    // Pick a representative day for the figure: a clear daily pattern with all
    // positive prices and a good spread. We score each day by its spread but
    // only among days whose minimum price is positive, so the arbitrage story
    // is easy to read (buy in the cheap trough, sell in the expensive peak).
    int best = 0;
    double best_spread = -1;
    for (size_t i = 0; i < days.size(); ++i) {
        double mn = days[i].prices[0], mx = days[i].prices[0];
        for (double p : days[i].prices) {
            mn = std::min(mn, p);
            mx = std::max(mx, p);
        }
        if (mn <= 5.0) continue;  // skip days with very low or negative prices
        if (mx - mn > best_spread) {
            best_spread = mx - mn;
            best = (int)i;
        }
    }
    const Day& d = days[best];
    Battery b = base_battery();
    Schedule dp = solve_dp(d.prices, b, N_SOC, SOC0, SOCT_MIN);

    auto snap = [](double x) { return std::abs(x) < 1e-9 ? 0.0 : x; };
    std::ofstream out("results/example_day.csv");
    out << "hour,price,soc,charge,discharge,net_action\n";
    for (int t = 0; t < 24; ++t) {
        double c = snap(dp.charge[t]);
        double dis = snap(dp.discharge[t]);
        double na = c - dis;  // + charge, - discharge
        out << t << "," << d.prices[t] << "," << snap(dp.soc[t]) << "," << c << ","
            << dis << "," << na << "\n";
    }
    std::cout << "Example day " << d.date << " spread " << best_spread
              << " net " << dp.net_profit << "\n";

    // Also export the greedy schedule for the same day, for the head to head
    // figure. We charge the same real wear cost so the numbers are comparable.
    Schedule gr = solve_greedy(d.prices, b, Q_LOW, Q_HIGH, SOC0);
    std::ofstream outg("results/example_day_greedy.csv");
    outg << "hour,price,soc,charge,discharge,net_action\n";
    for (int t = 0; t < 24; ++t) {
        double c = snap(gr.charge[t]);
        double dis = snap(gr.discharge[t]);
        outg << t << "," << d.prices[t] << "," << snap(gr.soc[t]) << "," << c
             << "," << dis << "," << (c - dis) << "\n";
    }
    // Summary line for the two methods on this day, for the figure caption.
    std::ofstream outs("results/example_day_compare.csv");
    outs << "method,net_profit,gross_revenue,throughput,full_cycles\n";
    outs << "dp," << dp.net_profit << "," << dp.gross_revenue << ","
         << dp.throughput << "," << dp.full_cycles << "\n";
    outs << "greedy," << gr.net_profit << "," << gr.gross_revenue << ","
         << gr.throughput << "," << gr.full_cycles << "\n";

    // Export the DP value table and the optimal path on a coarse grid so the
    // heatmap is readable. Rows are SoC levels (high SoC on top when plotted),
    // columns are stages t = 0..T.
    const int n_coarse = 21;
    std::vector<std::vector<double>> vtab;
    std::vector<int> path;
    solve_dp_with_table(d.prices, b, n_coarse, SOC0, SOCT_MIN, vtab, path);
    std::ofstream outv("results/example_day_vtable.csv");
    // Long format: stage, soc_level, value. Infeasible cells are written empty.
    outv << "stage,soc_level,value\n";
    for (int t = 0; t <= 24; ++t) {
        for (int s = 0; s < n_coarse; ++s) {
            double v = vtab[t][s];
            outv << t << "," << s << ",";
            if (v < -1e17) outv << "";  // infeasible
            else outv << v;
            outv << "\n";
        }
    }
    std::ofstream outp("results/example_day_path.csv");
    outp << "stage,soc_level\n";
    for (int t = 0; t <= 24; ++t) outp << t << "," << path[t] << "\n";
    std::cout << "Exported greedy, compare, vtable, path for the example day\n";
}

int main(int argc, char** argv) {
    std::string path = "data/processed/prices_2019.csv";
    if (argc > 1) path = argv[1];
    std::vector<Day> days = read_prices(path);
    if (days.empty()) {
        std::cerr << "No data loaded. Run the Python pipeline first.\n";
        return 1;
    }
    std::cout << "Loaded " << days.size() << " days from " << path << "\n";

    run_e1(days);
    run_e2(days);
    run_e3(days);
    run_e4(days);
    export_example_day(days);

    std::cout << "All experiments done. Results in results/.\n";
    return 0;
}
