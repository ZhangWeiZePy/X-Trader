#pragma once

#include <deque>
#include <cstdint>

class cancel_rate_guard
{
public:
	struct config
	{
		double max_cancel_ratio = 0.35;
		int64_t window_ms = 60000;
		int min_order_count = 10;
	};

public:
	cancel_rate_guard() {}
	explicit cancel_rate_guard(const config& cfg) : _cfg(cfg) {}

	void set_config(const config& cfg) { _cfg = cfg; }
	void on_order_event(int64_t ts_ms, bool is_cancel);
	bool allow_new_order(int64_t ts_ms);

private:
	void cleanup(int64_t ts_ms);

private:
	config _cfg;
	std::deque<std::pair<int64_t, bool>> _events;
	int _total = 0;
	int _cancelled = 0;
};
