#pragma once

#include "strategy.h"

class board_queue : public strategy
{
public:
    board_queue(stratid_t id, frame &frame) :
        strategy(id, frame)
    {}

    ~board_queue()
    {}

    virtual bool set_config(const std::map<std::string, std::string> &config) override;

    virtual void on_init() override;

    virtual void on_tick(const OrderBookData &tick) override;

    virtual void on_order(const Order &order) override;

    virtual void on_trade(const Order &order) override;

    virtual void on_cancel(const Order &order) override;

    virtual void on_error(const Order &order) override;

    virtual void on_update() override;

private:
    bool in_active_window(const std::string &hhmmss) const;

    bool should_enter(double board_amount, double board_lots) const;

    bool should_exit(double board_amount, double board_lots) const;

    bool compute_board_metrics(const OrderBookData &tick, double &board_amount, double &board_lots) const;

    void clear_active_order(orderref_t order_ref);

private:
    std::string _contract;
    eOrderFlag _order_flag = eOrderFlag::Limit;
    int _quantity = 0;
    std::string _active_start_time;
    std::string _active_end_time;

    bool _enable_queue_amount_enter = false;
    double _queue_amount_enter = 0.0;
    bool _enable_queue_lots_enter = false;
    double _queue_lots_enter = 0.0;
    bool _enable_queue_amount_exit = false;
    double _queue_amount_exit = 0.0;
    bool _enable_queue_lots_exit = false;
    double _queue_lots_exit = 0.0;

    bool _inited = false;
    bool _has_placed_once = false;
    orderref_t _active_orderref = null_orderref;
    std::string _latest_tick_time;
    double _latest_board_amount = 0.0;
    double _latest_board_lots = 0.0;
};
