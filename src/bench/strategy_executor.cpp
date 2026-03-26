#include "strategy_executor.h"
#include "market_data_receiver.h"
#include <deque>
#include <stdexcept>

namespace bench
{
class percent_change_benchmark_strategy final : public benchmark_strategy
{
public:
	explicit percent_change_benchmark_strategy(const BenchmarkConfig& cfg)
		: _buy_trigger_pct(cfg.buy_trigger_pct),
		_sell_trigger_pct(cfg.sell_trigger_pct),
		_default_volume(cfg.default_order_volume) {}

	void on_tick(const BenchmarkTick& tick, const signal_emitter_t& emit_signal) override
	{
		if (tick.pre_close_price <= 0 || tick.last_price <= 0) { return; }
		double change_pct = (tick.last_price - tick.pre_close_price) * 100.0 / tick.pre_close_price;
		if (change_pct >= _buy_trigger_pct)
		{
			emit_signal(OrderCommand{ OrderSide::Buy, tick.last_price, _default_volume });
		}
		else if (change_pct <= _sell_trigger_pct)
		{
			emit_signal(OrderCommand{ OrderSide::Sell, tick.last_price, _default_volume });
		}
	}

	void reset() override {}

private:
	double _buy_trigger_pct;
	double _sell_trigger_pct;
	uint32_t _default_volume;
};

class ma_cross_benchmark_strategy final : public benchmark_strategy
{
public:
	explicit ma_cross_benchmark_strategy(const BenchmarkConfig& cfg)
		: _fast_window(cfg.ma_fast_window),
		_slow_window(cfg.ma_slow_window),
		_default_volume(cfg.default_order_volume),
		_fast_sum(0.0),
		_slow_sum(0.0),
		_last_signal(0) {}

	void on_tick(const BenchmarkTick& tick, const signal_emitter_t& emit_signal) override
	{
		update_window(_fast_prices, _fast_sum, tick.last_price, _fast_window);
		update_window(_slow_prices, _slow_sum, tick.last_price, _slow_window);

		if (_fast_prices.size() < _fast_window || _slow_prices.size() < _slow_window) { return; }

		double fast_ma = _fast_sum / static_cast<double>(_fast_window);
		double slow_ma = _slow_sum / static_cast<double>(_slow_window);

		if (fast_ma > slow_ma && _last_signal <= 0)
		{
			_last_signal = 1;
			emit_signal(OrderCommand{ OrderSide::Buy, tick.last_price, _default_volume });
		}
		else if (fast_ma < slow_ma && _last_signal >= 0)
		{
			_last_signal = -1;
			emit_signal(OrderCommand{ OrderSide::Sell, tick.last_price, _default_volume });
		}
	}

	void reset() override
	{
		_fast_prices.clear();
		_slow_prices.clear();
		_fast_sum = 0.0;
		_slow_sum = 0.0;
		_last_signal = 0;
	}

private:
	static void update_window(std::deque<double>& queue, double& sum, double value, size_t window)
	{
		queue.push_back(value);
		sum += value;
		if (queue.size() > window)
		{
			sum -= queue.front();
			queue.pop_front();
		}
	}

	size_t _fast_window;
	size_t _slow_window;
	uint32_t _default_volume;
	std::deque<double> _fast_prices;
	std::deque<double> _slow_prices;
	double _fast_sum;
	double _slow_sum;
	int _last_signal;
};

std::unique_ptr<benchmark_strategy> create_strategy(const std::string& strategy_type, const BenchmarkConfig& cfg)
{
	if (strategy_type == "percent_change")
	{
		return std::make_unique<percent_change_benchmark_strategy>(cfg);
	}
	if (strategy_type == "ma_cross")
	{
		return std::make_unique<ma_cross_benchmark_strategy>(cfg);
	}
	throw std::runtime_error("unsupported strategy type: " + strategy_type);
}

strategy_executor::strategy_executor(const BenchmarkConfig& cfg, StrategyCase strategy_case)
	: _cfg(cfg), _strategy_case(std::move(strategy_case))
{
}

RunResult strategy_executor::run_once(uint32_t run_index)
{
	auto strategy = create_strategy(_strategy_case.strategy_type, _cfg);
	market_data_receiver market(_cfg, _strategy_case.data_frequency_hz);

	for (uint32_t i = 0; i < _cfg.warmup_ticks; ++i)
	{
		const auto tick = market.next_tick();
		strategy->on_tick(tick, [](const OrderCommand&) {});
		market.pace_next();
	}

	strategy->reset();

	RunResult result;
	result.strategy_case = _strategy_case;
	result.run_index = run_index;
	result.tick_count = _cfg.ticks_per_run;
	result.samples.reserve(_cfg.ticks_per_run);

	for (uint32_t i = 0; i < _cfg.ticks_per_run; ++i)
	{
		const auto tick = market.next_tick();
		const uint64_t receive_ns = now_ns();

		bool signal_emitted = false;
		uint64_t signal_ns = 0;
		OrderCommand command;

		strategy->on_tick(tick, [&](const OrderCommand& order) {
			if (!signal_emitted)
			{
				command = order;
				signal_ns = now_ns();
				signal_emitted = true;
			}
		});

		if (signal_emitted)
		{
			LatencySample sample;
			sample.receive_ns = receive_ns;
			sample.signal_ns = signal_ns;
			sample.latency_ns = signal_ns >= receive_ns ? signal_ns - receive_ns : 0;
			result.samples.push_back(sample);
		}

		market.pace_next();
	}

	result.signal_count = result.samples.size();
	return result;
}
}
