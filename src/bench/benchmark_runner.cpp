#include "benchmark_runner.h"
#include "result_statistics.h"
#include "strategy_executor.h"
#include <iostream>

namespace bench
{
benchmark_runner::benchmark_runner(BenchmarkConfig config)
	: _config(std::move(config))
{
}

std::vector<CaseReport> benchmark_runner::run_all_cases() const
{
	std::vector<CaseReport> reports;
	for (const auto& strategy_type : _config.strategy_types)
	{
		for (uint32_t frequency_hz : _config.data_frequencies_hz)
		{
			StrategyCase strategy_case;
			strategy_case.strategy_type = strategy_type;
			strategy_case.data_frequency_hz = frequency_hz;

			std::vector<LatencySample> all_samples;
			size_t total_ticks = 0;
			size_t total_signals = 0;

			for (uint32_t run = 1; run <= _config.runs_per_case; ++run)
			{
				strategy_executor executor(_config, strategy_case);
				RunResult run_result = executor.run_once(run);
				total_ticks += run_result.tick_count;
				total_signals += run_result.signal_count;
				all_samples.insert(all_samples.end(), run_result.samples.begin(), run_result.samples.end());
			}

			CaseReport case_report;
			case_report.strategy_case = strategy_case;
			case_report.total_ticks = total_ticks;
			case_report.total_signals = total_signals;
			case_report.stats = compute_stats(all_samples);
			reports.push_back(case_report);

			std::cout
				<< "finished case strategy=" << strategy_case.strategy_type
				<< " frequency=" << strategy_case.data_frequency_hz
				<< "Hz runs=" << _config.runs_per_case
				<< " samples=" << case_report.stats.sample_count
				<< "\n";
		}
	}
	return reports;
}
}
