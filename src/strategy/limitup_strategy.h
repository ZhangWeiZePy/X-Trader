#pragma once

#include <chrono>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include "strategy.h"
#include "risk/risk_guard.h"
#include "risk/position_sizer.h"

class limitup_strategy : public strategy
{
public:
	limitup_strategy(stratid_t id, frame& frame);
	~limitup_strategy() {}

	bool set_config(const std::map<std::string, std::string>& config) override;
	void on_init() override;
	void on_tick(const MarketData& tick) override;
	void on_tbt_entrust(const TickByTickEntrustData& entrust) override;
	void on_tbt_trade(const TickByTickTradeData& trade) override;
	void on_order(const Order& order) override;
	void on_trade(const Order& order) override;
	void on_cancel(const Order& order) override;
	void on_error(const Order& order) override;
	void on_update() override;

private:
	enum class tempo_mode
	{
		first_only,
		reseal_only,
		first_and_reseal
	};

	enum class exec_mode
	{
		queue_first,
		fill_first,
		risk_first
	};

	struct symbol_state
	{
		bool ever_sealed = false;
		bool opened_since_seal = false;
		bool first_triggered = false;
		int64_t opened_ms = 0;
		int64_t cooldown_until_ms = 0;
		double day_high = 0;
		double day_low = 0;
	};

	struct pending_order
	{
		std::string symbol;
		std::chrono::steady_clock::time_point submit_tp;
	};

private:
	bool _inited = false;
	std::vector<std::string> _symbols;
	std::set<std::string> _symbol_set;
	std::unordered_map<std::string, symbol_state> _states;
	std::unordered_map<orderref_t, pending_order> _pending_orders;
	std::unordered_map<std::string, orderref_t> _active_order_by_symbol;
	std::unordered_map<std::string, int64_t> _last_signal_ms;
	std::unordered_map<std::string, double> _symbol_exposure;
	double _total_exposure = 0;

	std::string _market_mode = "mainboard";
	tempo_mode _tempo_mode = tempo_mode::first_and_reseal;
	exec_mode _exec_mode = exec_mode::queue_first;
	double _tick_epsilon = 0.0001;
	int _min_seal_lot = 100000;
	double _min_seal_amount = 0;
	int64_t _reseal_window_ms = 15000;
	double _max_pullback_bp = 80;
	int64_t _signal_cooldown_sec = 60;
	int _order_lot = 100;
	int _max_order_lots = 10;
	int64_t _queue_timeout_ms = 1200;
	risk_guard _risk_guard;
	position_sizer _position_sizer;

private:
	static std::string trim(const std::string& s);
	static std::vector<std::string> split_csv(const std::string& s);
	static int64_t parse_hms_ms(const char* hhmmss, int millisec);
	static bool is_symbol_mainboard(const std::string& symbol);
	bool is_market_allowed(const std::string& symbol) const;
	bool in_trade_window(int64_t ms_of_day) const;
	bool should_trigger(const MarketData& tick, symbol_state& st, int64_t ms_of_day);
	void trigger_buy(const std::string& symbol, const MarketData& tick, int64_t ms_of_day);
	void clear_order(orderref_t order_ref);
};
