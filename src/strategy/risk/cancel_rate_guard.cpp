#include "cancel_rate_guard.h"

void cancel_rate_guard::on_order_event(int64_t ts_ms, bool is_cancel)
{
	cleanup(ts_ms);
	_events.emplace_back(ts_ms, is_cancel);
	++_total;
	if (is_cancel) { ++_cancelled; }
}

bool cancel_rate_guard::allow_new_order(int64_t ts_ms)
{
	cleanup(ts_ms);
	if (_total < _cfg.min_order_count) { return true; }
	const double ratio = _total > 0 ? static_cast<double>(_cancelled) / static_cast<double>(_total) : 0.0;
	return ratio <= _cfg.max_cancel_ratio;
}

void cancel_rate_guard::cleanup(int64_t ts_ms)
{
	while (!_events.empty() && ts_ms - _events.front().first > _cfg.window_ms)
	{
		const auto is_cancel = _events.front().second;
		_events.pop_front();
		--_total;
		if (is_cancel) { --_cancelled; }
	}
	if (_total < 0) { _total = 0; }
	if (_cancelled < 0) { _cancelled = 0; }
}
