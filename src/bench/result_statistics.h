#pragma once

#include "bench_types.h"
#include <cstdint>
#include <vector>

namespace bench
{
struct LatencyStats
{
	size_t sample_count = 0;
	double mean_ns = 0.0;
	double stddev_ns = 0.0;
	uint64_t min_ns = 0;
	uint64_t max_ns = 0;
	uint64_t p50_ns = 0;
	uint64_t p90_ns = 0;
	uint64_t p95_ns = 0;
	uint64_t p99_ns = 0;
};

LatencyStats compute_stats(const std::vector<LatencySample>& samples);
}
