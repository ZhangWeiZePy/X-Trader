#pragma once

#include "config.h"
#include "report_generator.h"
#include <vector>

namespace bench
{
class benchmark_runner
{
public:
	explicit benchmark_runner(BenchmarkConfig config);
	std::vector<CaseReport> run_all_cases() const;

private:
	BenchmarkConfig _config;
};
}
