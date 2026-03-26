#pragma once

#include "bench_types.h"
#include "config.h"
#include <functional>
#include <memory>

namespace bench
{
using signal_emitter_t = std::function<void(const OrderCommand&)>;

class benchmark_strategy
{
public:
	virtual ~benchmark_strategy() = default;
	virtual void on_tick(const BenchmarkTick& tick, const signal_emitter_t& emit_signal) = 0;
	virtual void reset() = 0;
};

std::unique_ptr<benchmark_strategy> create_strategy(const std::string& strategy_type, const BenchmarkConfig& cfg);

class strategy_executor
{
public:
	strategy_executor(const BenchmarkConfig& cfg, StrategyCase strategy_case);
	RunResult run_once(uint32_t run_index);

private:
	const BenchmarkConfig& _cfg;
	StrategyCase _strategy_case;
};
}
