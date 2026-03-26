#pragma once

#include "strategy.h"

class percent_change_strategy : public strategy
{
public:
	percent_change_strategy(
		stratid_t id,
		frame& frame,
		std::string contract,
		double buy_trigger_pct,
		double sell_trigger_pct,
		uint32_t position_limit,
		int once_vol);
	~percent_change_strategy();

	virtual void on_tick(const MarketData& tick) override;
	virtual void on_order(const Order& order) override;
	virtual void on_trade(const Order& order) override;
	virtual void on_cancel(const Order& order) override;
	virtual void on_error(const Order& order) override;

private:
	std::string _contract;
	double _buy_trigger_pct;
	double _sell_trigger_pct;
	uint32_t _position_limit;
	int _once_vol;

	orderref_t _buy_orderref;
	orderref_t _sell_orderref;
	double _last_change_pct;
};
