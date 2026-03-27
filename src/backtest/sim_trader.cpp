#include "sim_trader.h"

#include <cstring>

orderref_t sim_trader::submit_buy_limit(const std::string& symbol, double price, int volume)
{
	if (symbol.empty() || price <= 0 || volume <= 0) { return null_orderref; }
	Order o{};
	o.event_flag = eEventFlag::Order;
	o.order_ref = _seq++;
	std::strncpy(o.instrument_id, symbol.c_str(), sizeof(o.instrument_id) - 1);
	o.dir_offset = eDirOffset::BuyOpen;
	o.order_flag = eOrderFlag::Limit;
	o.limit_price = price;
	o.volume_total_original = volume;
	o.volume_total = volume;
	o.order_status = eOrderStatus::NoTradeQueueing;
	_active_orders[o.order_ref] = o;
	emit_order(o);
	return o.order_ref;
}

bool sim_trader::cancel(orderref_t order_ref)
{
	auto it = _active_orders.find(order_ref);
	if (it == _active_orders.end()) { return false; }
	Order o = it->second;
	o.event_flag = eEventFlag::Cancel;
	o.order_status = eOrderStatus::Canceled;
	_active_orders.erase(it);
	emit_order(o);
	return true;
}

void sim_trader::on_tick(const MarketData& tick)
{
	for (auto it = _active_orders.begin(); it != _active_orders.end();)
	{
		Order o = it->second;
		if (std::strcmp(o.instrument_id, tick.instrument_id) != 0)
		{
			++it;
			continue;
		}
		const double deal_price = tick.ask_price[0] * (1.0 + _cfg.slippage_bp / 10000.0);
		if (deal_price > 0 && o.limit_price >= deal_price)
		{
			o.event_flag = eEventFlag::Trade;
			o.order_status = eOrderStatus::AllTraded;
			o.volume_traded = o.volume_total_original;
			o.volume_total = 0;
			it = _active_orders.erase(it);
			emit_order(o);
			continue;
		}
		++it;
	}
}

void sim_trader::emit_order(const Order& order)
{
	if (_event_callback) { _event_callback(order); }
}
