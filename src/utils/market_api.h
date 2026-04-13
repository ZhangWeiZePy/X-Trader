#pragma once

#include "event_center.hpp"
#include "data_struct.h"

class market_api : public event_ringbuffer<OrderBookData>
{
public:
    market_api() :
        _tbt_entrust_callback(nullptr), _tbt_trade_callback(nullptr), _orderbook_callback(nullptr)
    {}

    virtual ~market_api()
    {}

public:
    virtual void release() = 0;

public:
    ///绑定逐笔委托回调
    inline void bind_tbt_entrust_callback(const tbt_entrust_callback &callback)
    {
        _tbt_entrust_callback = callback;
    }

    ///绑定逐笔成交回调
    inline void bind_tbt_trade_callback(const tbt_trade_callback &callback)
    {
        _tbt_trade_callback = callback;
    }

    ///绑定本地订单薄回调
    inline void bind_orderbook_callback(const orderbook_callback &callback)
    {
        _orderbook_callback = callback;
    }

protected:
    ///派发逐笔委托事件
    inline void emit_tbt_entrust(const TickByTickEntrustData &entrust)
    {
        if (_tbt_entrust_callback)
        {
            _tbt_entrust_callback(entrust);
        }
    }

    ///派发逐笔成交事件
    inline void emit_tbt_trade(const TickByTickTradeData &trade)
    {
        if (_tbt_trade_callback)
        {
            _tbt_trade_callback(trade);
        }
    }

    ///派发本地订单薄事件
    inline void emit_orderbook(const OrderBookData &orderbook)
    {
        if (_orderbook_callback)
        {
            _orderbook_callback(orderbook);
        }
    }

private:
    ///逐笔委托回调
    tbt_entrust_callback _tbt_entrust_callback;
    ///逐笔成交回调
    tbt_trade_callback _tbt_trade_callback;
    ///本地订单薄回调
    orderbook_callback _orderbook_callback;

public:
    std::atomic<bool> _is_ready{false};
};
