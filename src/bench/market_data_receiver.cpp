#include "market_data_receiver.h"
#include <thread>

namespace bench
{
market_data_receiver::market_data_receiver(const BenchmarkConfig& cfg, uint32_t frequency_hz)
	: _cfg(cfg),
	_frequency_hz(frequency_hz),
	_sequence(0),
	_last_price(cfg.base_price),
	_rng(cfg.random_seed + frequency_hz),
	_noise(0.0, 1.0),
	_next_emit_time(steady_clock::now())
{
}

void market_data_receiver::reset()
{
	_sequence = 0;
	_last_price = _cfg.base_price;
	_rng.seed(_cfg.random_seed + _frequency_hz);
	_next_emit_time = steady_clock::now();
}

BenchmarkTick market_data_receiver::next_tick()
{
	++_sequence;
	double move_bps = _cfg.volatility_bps * _noise(_rng);
	double move_ratio = move_bps / 10000.0;
	_last_price = _last_price * (1.0 + move_ratio);
	if (_last_price <= 0.01) { _last_price = 0.01; }

	BenchmarkTick tick;
	tick.sequence = _sequence;
	tick.pre_close_price = _cfg.base_price;
	tick.last_price = _last_price;
	return tick;
}

void market_data_receiver::pace_next()
{
	if (_cfg.burst_mode || _frequency_hz == 0) { return; }
	auto interval = std::chrono::nanoseconds(1000000000ULL / _frequency_hz);
	_next_emit_time += interval;
	std::this_thread::sleep_until(_next_emit_time);
}
}
