#include "replay_runner.h"

void replay_runner::set_strategies(const std::vector<std::shared_ptr<strategy>>& strategys)
{
	_strategys = strategys;
}

bool replay_runner::load_csv(const std::string& path)
{
	return _reader.open(path);
}

replay_runner::result replay_runner::run()
{
	_result = result{};
	_sim_trader.set_event_callback([this](const Order& order)->void { on_order_event(order); });
	for (auto& s : _strategys)
	{
		if (s) { s->on_init(); }
	}
	MarketData tick{};
	while (_reader.read_next(tick))
	{
		++_result.tick_count;
		_sim_trader.on_tick(tick);
		for (auto& s : _strategys)
		{
			if (s) { s->on_tick(tick); }
		}
		for (auto& s : _strategys)
		{
			if (s) { s->on_update(); }
		}
	}
	return _result;
}

void replay_runner::on_order_event(const Order& order)
{
	++_result.order_event_count;
	if (order.event_flag == eEventFlag::Trade) { ++_result.trade_event_count; }
	if (order.event_flag == eEventFlag::Cancel) { ++_result.cancel_event_count; }
	for (auto& s : _strategys)
	{
		if (!s) { continue; }
		if (order.event_flag == eEventFlag::Trade) { s->on_trade(order); }
		else if (order.event_flag == eEventFlag::Cancel) { s->on_cancel(order); }
		else if (order.event_flag == eEventFlag::ErrorInsert || order.event_flag == eEventFlag::ErrorCancel) { s->on_error(order); }
		else { s->on_order(order); }
	}
}
