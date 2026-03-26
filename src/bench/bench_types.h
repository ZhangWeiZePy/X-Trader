#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace bench
{
using steady_clock = std::chrono::steady_clock;

inline uint64_t now_ns()
{
	return static_cast<uint64_t>(
		std::chrono::duration_cast<std::chrono::nanoseconds>(
			steady_clock::now().time_since_epoch()).count());
}

enum class OrderSide : uint8_t
{
	Buy = 0,
	Sell = 1
};

struct BenchmarkTick
{
	uint64_t sequence = 0;
	double pre_close_price = 0.0;
	double last_price = 0.0;
};

struct OrderCommand
{
	OrderSide side = OrderSide::Buy;
	double price = 0.0;
	uint32_t volume = 0;
};

struct LatencySample
{
	uint64_t receive_ns = 0;
	uint64_t signal_ns = 0;
	uint64_t latency_ns = 0;
};

struct StrategyCase
{
	std::string strategy_type;
	uint32_t data_frequency_hz = 1000;
};

struct RunResult
{
	StrategyCase strategy_case;
	uint32_t run_index = 0;
	size_t tick_count = 0;
	size_t signal_count = 0;
	std::vector<LatencySample> samples;
};
}
