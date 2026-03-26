#include "config.h"
#include "INIReader.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace bench
{
static std::string trim(const std::string& input)
{
	size_t start = 0;
	while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) { ++start; }
	size_t end = input.size();
	while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) { --end; }
	return input.substr(start, end - start);
}

static std::vector<std::string> split_csv_strings(const std::string& text)
{
	std::vector<std::string> values;
	std::stringstream ss(text);
	std::string item;
	while (std::getline(ss, item, ','))
	{
		std::string t = trim(item);
		if (!t.empty()) { values.push_back(t); }
	}
	return values;
}

static std::vector<uint32_t> split_csv_uint32(const std::string& text)
{
	std::vector<uint32_t> values;
	for (const auto& token : split_csv_strings(text))
	{
		uint32_t value = static_cast<uint32_t>(std::strtoul(token.c_str(), nullptr, 10));
		if (value > 0) { values.push_back(value); }
	}
	return values;
}

BenchmarkConfig load_config(const std::string& ini_file)
{
	BenchmarkConfig cfg;
	cfg.strategy_types = { "percent_change" };
	cfg.data_frequencies_hz = { 1000 };

	INIReader reader(ini_file);
	if (reader.ParseError() < 0)
	{
		return cfg;
	}

	std::string strategy_types_text = reader.Get("benchmark", "strategy_types", "percent_change");
	std::string frequencies_text = reader.Get("benchmark", "data_frequencies_hz", "1000");

	cfg.strategy_types = split_csv_strings(strategy_types_text);
	cfg.data_frequencies_hz = split_csv_uint32(frequencies_text);
	if (cfg.strategy_types.empty()) { cfg.strategy_types = { "percent_change" }; }
	if (cfg.data_frequencies_hz.empty()) { cfg.data_frequencies_hz = { 1000 }; }

	cfg.runs_per_case = static_cast<uint32_t>(reader.GetInteger("benchmark", "runs_per_case", cfg.runs_per_case));
	cfg.warmup_ticks = static_cast<uint32_t>(reader.GetInteger("benchmark", "warmup_ticks", cfg.warmup_ticks));
	cfg.ticks_per_run = static_cast<uint32_t>(reader.GetInteger("benchmark", "ticks_per_run", cfg.ticks_per_run));
	cfg.burst_mode = reader.GetBoolean("benchmark", "burst_mode", cfg.burst_mode);
	cfg.base_price = reader.GetReal("benchmark", "base_price", cfg.base_price);
	cfg.volatility_bps = reader.GetReal("benchmark", "volatility_bps", cfg.volatility_bps);
	cfg.random_seed = static_cast<uint32_t>(reader.GetInteger("benchmark", "random_seed", cfg.random_seed));
	cfg.default_order_volume = static_cast<uint32_t>(reader.GetInteger("benchmark", "default_order_volume", cfg.default_order_volume));
	cfg.buy_trigger_pct = reader.GetReal("benchmark", "buy_trigger_pct", cfg.buy_trigger_pct);
	cfg.sell_trigger_pct = reader.GetReal("benchmark", "sell_trigger_pct", cfg.sell_trigger_pct);
	cfg.ma_fast_window = static_cast<uint32_t>(reader.GetInteger("benchmark", "ma_fast_window", cfg.ma_fast_window));
	cfg.ma_slow_window = static_cast<uint32_t>(reader.GetInteger("benchmark", "ma_slow_window", cfg.ma_slow_window));
	cfg.report_output_file = reader.Get("benchmark", "report_output_file", cfg.report_output_file);

	cfg.runs_per_case = std::max<uint32_t>(1, cfg.runs_per_case);
	cfg.ticks_per_run = std::max<uint32_t>(1, cfg.ticks_per_run);
	cfg.default_order_volume = std::max<uint32_t>(1, cfg.default_order_volume);
	cfg.ma_fast_window = std::max<uint32_t>(1, cfg.ma_fast_window);
	cfg.ma_slow_window = std::max<uint32_t>(cfg.ma_fast_window + 1, cfg.ma_slow_window);
	if (cfg.sell_trigger_pct > 0) { cfg.sell_trigger_pct = -std::abs(cfg.sell_trigger_pct); }
	if (cfg.report_output_file.empty()) { cfg.report_output_file = "benchmark_report.csv"; }

	return cfg;
}
}
