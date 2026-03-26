#pragma once

#include "bench_types.h"
#include "result_statistics.h"
#include <string>
#include <vector>

namespace bench
{
struct CaseReport
{
	StrategyCase strategy_case;
	size_t total_ticks = 0;
	size_t total_signals = 0;
	LatencyStats stats;
};

class report_generator
{
public:
	static void print_to_console(const std::vector<CaseReport>& reports);
	static bool write_csv(const std::string& output_file, const std::vector<CaseReport>& reports);
};
}
