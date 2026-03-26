#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bench
{
struct BenchmarkConfig
{
	std::vector<std::string> strategy_types;
	std::vector<uint32_t> data_frequencies_hz;
	uint32_t runs_per_case = 10;
	uint32_t warmup_ticks = 1000;
	uint32_t ticks_per_run = 100000;
	bool burst_mode = true;
	double base_price = 100.0;
	double volatility_bps = 15.0;
	uint32_t random_seed = 42;
	uint32_t default_order_volume = 100;
	double buy_trigger_pct = 1.0;
	double sell_trigger_pct = -1.0;
	uint32_t ma_fast_window = 8;
	uint32_t ma_slow_window = 32;
	std::string report_output_file = "benchmark_report.csv";
};

BenchmarkConfig load_config(const std::string& ini_file);
}
