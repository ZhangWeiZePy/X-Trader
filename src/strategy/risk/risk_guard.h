#pragma once

#include <string>
#include <unordered_map>
#include "cancel_rate_guard.h"

class risk_guard
{
public:
	struct config
	{
		double max_total_exposure = 2000000;
		double max_capital_per_symbol = 300000;
		int max_signals_per_day = 30;
		int64_t no_new_position_after_ms = (14 * 3600 + 50 * 60) * 1000;
		cancel_rate_guard::config cancel_cfg;
	};

public:
	risk_guard() {}
	explicit risk_guard(const config& cfg) : _cfg(cfg), _cancel_guard(cfg.cancel_cfg) {}

	void set_config(const config& cfg);
	void reset_day();
	bool can_open(const std::string& symbol, int64_t now_ms, double total_exposure, double symbol_exposure) const;
	void on_signal();
	bool reach_signal_limit() const;
	cancel_rate_guard& cancel_guard() { return _cancel_guard; }
	const config& get_config() const { return _cfg; }

private:
	config _cfg;
	int _signal_count = 0;
	cancel_rate_guard _cancel_guard;
};
