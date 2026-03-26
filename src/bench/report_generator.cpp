#include "report_generator.h"
#include <fstream>
#include <iomanip>
#include <iostream>

namespace bench
{
void report_generator::print_to_console(const std::vector<CaseReport>& reports)
{
	std::cout << "Benchmark Result\n";
	std::cout
		<< std::left
		<< std::setw(18) << "strategy"
		<< std::setw(14) << "frequency_hz"
		<< std::setw(14) << "total_ticks"
		<< std::setw(16) << "total_signals"
		<< std::setw(12) << "mean_ns"
		<< std::setw(12) << "stddev_ns"
		<< std::setw(10) << "min_ns"
		<< std::setw(10) << "p50_ns"
		<< std::setw(10) << "p90_ns"
		<< std::setw(10) << "p95_ns"
		<< std::setw(10) << "p99_ns"
		<< std::setw(10) << "max_ns"
		<< "\n";
	std::cout << std::string(146, '-') << "\n";
	for (const auto& report : reports)
	{
		std::cout
			<< std::left
			<< std::setw(18) << report.strategy_case.strategy_type
			<< std::setw(14) << report.strategy_case.data_frequency_hz
			<< std::setw(14) << report.total_ticks
			<< std::setw(16) << report.total_signals
			<< std::setw(12) << std::fixed << std::setprecision(2) << report.stats.mean_ns
			<< std::setw(12) << report.stats.stddev_ns
			<< std::setw(10) << report.stats.min_ns
			<< std::setw(10) << report.stats.p50_ns
			<< std::setw(10) << report.stats.p90_ns
			<< std::setw(10) << report.stats.p95_ns
			<< std::setw(10) << report.stats.p99_ns
			<< std::setw(10) << report.stats.max_ns
			<< "\n";
	}
}

bool report_generator::write_csv(const std::string& output_file, const std::vector<CaseReport>& reports)
{
	std::ofstream ofs(output_file, std::ios::out | std::ios::trunc);
	if (!ofs.is_open()) { return false; }

	ofs << "strategy,frequency_hz,total_ticks,total_signals,sample_count,mean_ns,stddev_ns,min_ns,p50_ns,p90_ns,p95_ns,p99_ns,max_ns\n";
	for (const auto& report : reports)
	{
		ofs
			<< report.strategy_case.strategy_type << ","
			<< report.strategy_case.data_frequency_hz << ","
			<< report.total_ticks << ","
			<< report.total_signals << ","
			<< report.stats.sample_count << ","
			<< std::fixed << std::setprecision(2) << report.stats.mean_ns << ","
			<< report.stats.stddev_ns << ","
			<< report.stats.min_ns << ","
			<< report.stats.p50_ns << ","
			<< report.stats.p90_ns << ","
			<< report.stats.p95_ns << ","
			<< report.stats.p99_ns << ","
			<< report.stats.max_ns
			<< "\n";
	}
	return true;
}
}
