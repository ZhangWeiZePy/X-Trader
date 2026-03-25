#pragma once

#define NT 3

#include <float.h>
#include "strategy.h"
#include "resample.h"

class market_correction : public strategy, public bar_receiver
{
	enum class eStatus
	{
		Peak,///波峰
		Trough,///波谷
		Rise,///涨
		Decline,///跌
		Oscillation,///震荡
		Unknown,///未知
	};

public:
	market_correction(stratid_t id, frame& frame, std::string contract, uint32_t period);
	~market_correction();
	
	virtual void on_init() {}
	virtual void on_tick(const MarketData& tick) override;
	virtual void on_bar(const Sample& bar) override;
	virtual void on_order(const Order& order) override;
	virtual void on_trade(const Order& order) override;
	virtual void on_cancel(const Order& order) override;
	virtual void on_error(const Order& order) override;
	virtual void on_update() override;

private:
	std::string _contract;
	uint32_t _period;
	orderref_t _buy_orderref = null_orderref;
	orderref_t _sell_orderref = null_orderref;

	bool _is_closing = false;
	
	double _ask_bid[NT]{};
	int _index_t = 0;
	MarketData _last_tick{};
	Sample _last_bar;

	eStatus _status = eStatus::Unknown;

	int _once_vol = 1;
	int _cancel_count = 0;
};
