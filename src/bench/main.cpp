#include "benchmark_runner.h"
#include "config.h"
#include "report_generator.h"
#include <iostream>

int main(int argc, char** argv)
{
	std::string ini_file = "./bench/benchmark.sample.ini";
	if (argc > 1 && argv[1] != nullptr)
	{
		ini_file = argv[1];
	}

	bench::BenchmarkConfig cfg = bench::load_config(ini_file);
	bench::benchmark_runner runner(cfg);
	auto reports = runner.run_all_cases();
	bench::report_generator::print_to_console(reports);

	bool ok = bench::report_generator::write_csv(cfg.report_output_file, reports);
	if (!ok)
	{
		std::cerr << "failed to write report file: " << cfg.report_output_file << "\n";
		return 2;
	}

	std::cout << "report file: " << cfg.report_output_file << "\n";
	return 0;
}
