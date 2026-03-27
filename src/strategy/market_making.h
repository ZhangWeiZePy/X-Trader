#pragma once

#include "strategy.h"


class market_making : public strategy
{
public:
	market_making(stratid_t id, frame& frame) : strategy(id, frame) {}

	~market_making() {}

	virtual bool set_config(const std::map<std::string, std::string>& config) override;
	virtual void on_init() override;
	virtual void on_tick(const MarketData& tick) override;
	virtual void on_order(const Order& order) override;
	virtual void on_trade(const Order& order) override;
	virtual void on_cancel(const Order& order) override;
	virtual void on_error(const Order& order) override;

private:
	std::string _contract;
	double _price_delta = 0;
	uint32_t _position_limit = 0;
	int _once_vol = 0;
	orderref_t _buy_orderref = null_orderref;
	orderref_t _sell_orderref = null_orderref;
	bool _is_closing = false;
	bool _inited = false;
};
