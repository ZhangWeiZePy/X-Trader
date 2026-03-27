#pragma once

#include <memory>
#include <vector>
#include "strategy.h"
#include "tick_reader.h"
#include "sim_trader.h"

class replay_runner
{
public:
	struct result
	{
		size_t tick_count = 0;
		size_t order_event_count = 0;
		size_t trade_event_count = 0;
		size_t cancel_event_count = 0;
	};

public:
	replay_runner() {}
	void set_sim_config(const sim_trader::config& cfg) { _sim_trader.set_config(cfg); }
	void set_strategies(const std::vector<std::shared_ptr<strategy>>& strategys);
	bool load_csv(const std::string& path);
	result run();

private:
	void on_order_event(const Order& order);

private:
	tick_reader _reader;
	sim_trader _sim_trader;
	std::vector<std::shared_ptr<strategy>> _strategys;
	result _result;
};
