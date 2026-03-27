#pragma once

#include "event_center.hpp"
#include "data_struct.h"

class market_api : public event_ringbuffer<MarketData>
{
public:
	market_api() : _tbt_entrust_callback(nullptr), _tbt_trade_callback(nullptr) {}
	virtual ~market_api() {}

public:
	virtual void release() = 0;

public:
	///绑定逐笔委托回调
	inline void bind_tbt_entrust_callback(const std::function<void(const TickByTickEntrustData&)>& callback)
	{
		_tbt_entrust_callback = callback;
	}

	///绑定逐笔成交回调
	inline void bind_tbt_trade_callback(const std::function<void(const TickByTickTradeData&)>& callback)
	{
		_tbt_trade_callback = callback;
	}

protected:
	///派发逐笔委托事件
	inline void emit_tbt_entrust(const TickByTickEntrustData& entrust)
	{
		if (_tbt_entrust_callback) { _tbt_entrust_callback(entrust); }
	}

	///派发逐笔成交事件
	inline void emit_tbt_trade(const TickByTickTradeData& trade)
	{
		if (_tbt_trade_callback) { _tbt_trade_callback(trade); }
	}

private:
	///逐笔委托回调
	std::function<void(const TickByTickEntrustData&)> _tbt_entrust_callback;
	///逐笔成交回调
	std::function<void(const TickByTickTradeData&)> _tbt_trade_callback;

public:
	std::atomic<bool> _is_ready{ false };
};
