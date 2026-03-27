#include "risk_guard.h"

void risk_guard::set_config(const config& cfg)
{
	_cfg = cfg;
	_cancel_guard.set_config(cfg.cancel_cfg);
}

void risk_guard::reset_day()
{
	_signal_count = 0;
}

bool risk_guard::can_open(const std::string& symbol, int64_t now_ms, double total_exposure, double symbol_exposure) const
{
	if (symbol.empty()) { return false; }
	if (now_ms > _cfg.no_new_position_after_ms) { return false; }
	if (total_exposure >= _cfg.max_total_exposure) { return false; }
	if (symbol_exposure >= _cfg.max_capital_per_symbol) { return false; }
	if (_signal_count >= _cfg.max_signals_per_day) { return false; }
	return true;
}

void risk_guard::on_signal()
{
	++_signal_count;
}

bool risk_guard::reach_signal_limit() const
{
	return _signal_count >= _cfg.max_signals_per_day;
}
