#pragma once

#include "strategy.h"

class board_queue : public strategy
{
public:
    struct config_strategy_board_queue
    {
        std::string contract;
        eOrderFlag order_flag{eOrderFlag::Limit};
        int quantity{0};
        std::string active_start_time;
        std::string active_end_time;
        bool enable_queue_amount_enter{false};
        double queue_amount_enter{0.0};
        bool enable_queue_lots_enter{false};
        double queue_lots_enter{0.0};
        bool enable_queue_amount_exit{false};
        double queue_amount_exit{0.0};
        bool enable_queue_lots_exit{false};
        double queue_lots_exit{0.0};
        bool allow_reenter_after_cancel{false};
        int max_reenter_times{0};
        NLOHMANN_DEFINE_TYPE_INTRUSIVE(config_strategy_board_queue, contract, order_flag, quantity, active_start_time, active_end_time, enable_queue_amount_enter, queue_amount_enter, enable_queue_lots_enter, queue_lots_enter, enable_queue_amount_exit, queue_amount_exit, enable_queue_lots_exit, queue_lots_exit, allow_reenter_after_cancel, max_reenter_times)
    };

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
    config_strategy_board_queue _cfg{};

    bool _inited = false;
    bool _has_placed_once = false;
    int _reenter_used_times = 0;
    bool _pending_reenter_after_cancel = false;
    orderref_t _active_orderref = null_orderref;
    std::string _latest_tick_time;
    double _latest_board_amount = 0.0;
    double _latest_board_lots = 0.0;
};
