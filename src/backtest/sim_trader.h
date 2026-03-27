#pragma once

#include <functional>
#include <map>
#include "data_struct.h"

class sim_trader
{
public:
	struct config
	{
		double slippage_bp = 0;
	};

public:
	sim_trader() {}
	explicit sim_trader(const config& cfg) : _cfg(cfg) {}

	void set_config(const config& cfg) { _cfg = cfg; }
	void set_event_callback(std::function<void(const Order&)> callback) { _event_callback = callback; }
	orderref_t submit_buy_limit(const std::string& symbol, double price, int volume);
	bool cancel(orderref_t order_ref);
	void on_tick(const MarketData& tick);

private:
	void emit_order(const Order& order);

private:
	config _cfg;
	orderref_t _seq = 1;
	std::map<orderref_t, Order> _active_orders;
	std::function<void(const Order&)> _event_callback;
};
