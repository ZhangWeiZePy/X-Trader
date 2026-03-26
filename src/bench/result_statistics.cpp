#include "result_statistics.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace bench
{
static uint64_t percentile(const std::vector<uint64_t>& values, double ratio)
{
	if (values.empty()) { return 0; }
	double rank = ratio * static_cast<double>(values.size() - 1);
	size_t index = static_cast<size_t>(std::round(rank));
	return values[index];
}

LatencyStats compute_stats(const std::vector<LatencySample>& samples)
{
	LatencyStats stats;
	stats.sample_count = samples.size();
	if (samples.empty()) { return stats; }

	std::vector<uint64_t> latencies;
	latencies.reserve(samples.size());
	for (const auto& sample : samples)
	{
		latencies.push_back(sample.latency_ns);
	}

	std::sort(latencies.begin(), latencies.end());
	stats.min_ns = latencies.front();
	stats.max_ns = latencies.back();
	stats.p50_ns = percentile(latencies, 0.50);
	stats.p90_ns = percentile(latencies, 0.90);
	stats.p95_ns = percentile(latencies, 0.95);
	stats.p99_ns = percentile(latencies, 0.99);

	double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
	stats.mean_ns = sum / static_cast<double>(latencies.size());

	double variance_sum = 0.0;
	for (auto latency : latencies)
	{
		double diff = static_cast<double>(latency) - stats.mean_ns;
		variance_sum += diff * diff;
	}
	stats.stddev_ns = std::sqrt(variance_sum / static_cast<double>(latencies.size()));

	return stats;
}
}
