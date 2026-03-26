#include "percent_change_strategy.h"
#include <algorithm>

percent_change_strategy::percent_change_strategy(
	stratid_t id,
	frame& frame,
	std::string contract,
	double buy_trigger_pct,
	double sell_trigger_pct,
	uint32_t position_limit,
	int once_vol)
	: strategy(id, frame),
	_contract(contract),
	_buy_trigger_pct(buy_trigger_pct),
	_sell_trigger_pct(sell_trigger_pct),
	_position_limit(position_limit),
	_once_vol(once_vol),
	_buy_orderref(null_orderref),
	_sell_orderref(null_orderref),
	_last_change_pct(0.0)
{
	get_contracts().insert(contract);
}

percent_change_strategy::~percent_change_strategy() {}

void percent_change_strategy::on_tick(const MarketData& tick)
{
	double ref_price = tick.pre_close_price > 0 ? tick.pre_close_price : tick.pre_settlement_price;
	if (ref_price <= 0 || tick.last_price <= 0) { return; }

	_last_change_pct = (tick.last_price - ref_price) * 100.0 / ref_price;
	const auto& posi = get_position(_contract);

	if (_buy_orderref == null_orderref &&
		_last_change_pct >= _buy_trigger_pct &&
		posi.long_.position + posi.long_.open_no_trade < _position_limit)
	{
		int buy_vol = std::min(static_cast<int>(_position_limit - posi.long_.position - posi.long_.open_no_trade), _once_vol);
		if (buy_vol > 0)
		{
			double buy_price = tick.ask_price[0] > 0 ? tick.ask_price[0] : tick.last_price;
			_buy_orderref = buy_open(eOrderFlag::Limit, _contract, buy_price, buy_vol);
		}
	}

	if (_sell_orderref == null_orderref &&
		_last_change_pct <= _sell_trigger_pct &&
		posi.long_.closeable > 0)
	{
		int sell_vol = std::min(posi.long_.closeable, _once_vol);
		double sell_price = tick.bid_price[0] > 0 ? tick.bid_price[0] : tick.last_price;
		_sell_orderref = sell_close(eOrderFlag::Limit, _contract, sell_price, sell_vol);
	}
}

void percent_change_strategy::on_order(const Order& order)
{
	if (order.order_ref == _buy_orderref)
	{
		set_cancel_condition(order.order_ref, [this](orderref_t)->bool {
			return _last_change_pct < _buy_trigger_pct;
		});
	}
	else if (order.order_ref == _sell_orderref)
	{
		set_cancel_condition(order.order_ref, [this](orderref_t)->bool {
			return _last_change_pct > _sell_trigger_pct;
		});
	}
}

void percent_change_strategy::on_trade(const Order& order)
{
	if (_buy_orderref == order.order_ref)
	{
		_buy_orderref = null_orderref;
	}
	else if (_sell_orderref == order.order_ref)
	{
		_sell_orderref = null_orderref;
	}
}

void percent_change_strategy::on_cancel(const Order& order)
{
	if (_buy_orderref == order.order_ref)
	{
		_buy_orderref = null_orderref;
	}
	else if (_sell_orderref == order.order_ref)
	{
		_sell_orderref = null_orderref;
	}
}

void percent_change_strategy::on_error(const Order& order)
{
	if (_buy_orderref == order.order_ref)
	{
		_buy_orderref = null_orderref;
	}
	else if (_sell_orderref == order.order_ref)
	{
		_sell_orderref = null_orderref;
	}
}
