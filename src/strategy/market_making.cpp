#include "market_making.h"

#include <algorithm>
#include <cstdio>


bool market_making::set_config(const std::map<std::string, std::string> &config)
{
    if (!strategy::set_config(config))
    {
        return false;
    }
    const auto &contract = get_config("contract");
    if (contract.empty())
    {
        printf("market_making config missing contract\n");
        return false;
    }
    _contract = contract;
    get_contracts().clear();
    get_contracts().insert(_contract);
    return true;
}

void market_making::on_init()
{
    const auto &contract = get_config("contract");
    const auto &price_delta = get_config("price_delta");
    const auto &position_limit = get_config("position_limit");
    const auto &once_vol = get_config("once_vol");
    if (contract.empty() || price_delta.empty() || position_limit.empty() || once_vol.empty())
    {
        printf("market_making config fields are required: contract, price_delta, position_limit, once_vol\n");
        return;
    }
    try
    {
        _contract = contract;
        _price_delta = std::stod(price_delta);
        _position_limit = static_cast<uint32_t>(std::stoul(position_limit));
        _once_vol = std::stoi(once_vol);
        if (_once_vol <= 0 || _position_limit == 0)
        {
            printf("market_making config value out of range\n");
            return;
        }
        _inited = true;
    } catch (...)
    {
        printf("market_making config parse failed\n");
    }
}


void market_making::on_tick(const MarketData &tick)
{
    if (!_inited)
    {
        return;
    }
    if (strncmp(tick.update_time, "14:59", 5) == 0)
    {
        _is_closing = true;
    }

    const auto &posi = get_position(_contract);

    if (_is_closing)
    {
        if (posi.long_.closeable)
        {
            sell_close_today(eOrderFlag::Limit, _contract, tick.bid_price[0], posi.long_.closeable);
        }
        if (posi.short_.closeable)
        {
            buy_close_today(eOrderFlag::Limit, _contract, tick.ask_price[0], posi.short_.closeable);
        }
        return;
    }


    if (_buy_orderref == null_orderref)
    {
        double buy_price = tick.bid_price[0] - _price_delta;

        if (posi.short_.his_closeable > 0)
        {
            uint32_t buy_vol = std::min(posi.short_.his_closeable, _once_vol);
            _buy_orderref = buy_close_yesterday(eOrderFlag::Limit, _contract, buy_price, buy_vol);
        } else if (posi.short_.today_closeable > 0)
        {
            uint32_t buy_vol = std::min(posi.short_.today_closeable, _once_vol);
            _buy_orderref = buy_close_today(eOrderFlag::Limit, _contract, buy_price, buy_vol);
        } else if (posi.long_.position + posi.long_.open_no_trade < _position_limit)
        {
            _buy_orderref = buy_open(eOrderFlag::Limit, _contract, buy_price, _once_vol);
        }
    }

    if (_sell_orderref == null_orderref)
    {
        double sell_price = tick.ask_price[0] + _price_delta;

        if (posi.long_.his_closeable > 0)
        {
            uint32_t sell_vol = std::min(posi.long_.his_closeable, _once_vol);
            _sell_orderref = sell_close_yesterday(eOrderFlag::Limit, _contract, sell_price, sell_vol);
        } else if (posi.long_.today_closeable > 0)
        {
            uint32_t sell_vol = std::min(posi.long_.today_closeable, _once_vol);
            _sell_orderref = sell_close_today(eOrderFlag::Limit, _contract, sell_price, sell_vol);
        } else if (posi.short_.position + posi.short_.open_no_trade < _position_limit)
        {
            _sell_orderref = sell_open(eOrderFlag::Limit, _contract, sell_price, _once_vol);
        }
    }
}

void market_making::on_order(const Order &order)
{
    if (order.order_ref == _buy_orderref || order.order_ref == _sell_orderref)
    {
        set_cancel_condition(order.order_ref, [this](orderref_t order_id)-> bool
        {
            if (_is_closing)
            {
                return true;
            }
            return false;
        });
    }
}

void market_making::on_trade(const Order &order)
{
    if (_buy_orderref == order.order_ref)
    {
        cancel_order(_sell_orderref);
        _buy_orderref = null_orderref;
    }
    if (_sell_orderref == order.order_ref)
    {
        cancel_order(_buy_orderref);
        _sell_orderref = null_orderref;
    }
}

void market_making::on_cancel(const Order &order)
{
    if (_buy_orderref == order.order_ref)
    {
        _buy_orderref = null_orderref;
    }

    if (_sell_orderref == order.order_ref)
    {
        _sell_orderref = null_orderref;
    }
}

void market_making::on_error(const Order &order)
{
    if (_buy_orderref == order.order_ref)
    {
        _buy_orderref = null_orderref;
    }

    if (_sell_orderref == order.order_ref)
    {
        _sell_orderref = null_orderref;
    }
}
