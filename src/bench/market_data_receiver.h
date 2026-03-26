#pragma once

#include "bench_types.h"
#include "config.h"
#include <random>

namespace bench
{
class market_data_receiver
{
public:
	market_data_receiver(const BenchmarkConfig& cfg, uint32_t frequency_hz);

	void reset();
	BenchmarkTick next_tick();
	void pace_next();

private:
	const BenchmarkConfig& _cfg;
	uint32_t _frequency_hz;
	uint64_t _sequence;
	double _last_price;
	std::mt19937_64 _rng;
	std::normal_distribution<double> _noise;
	steady_clock::time_point _next_emit_time;
};
}
