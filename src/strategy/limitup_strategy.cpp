#include "limitup_strategy.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

limitup_strategy::limitup_strategy(stratid_t id, frame& frame) : strategy(id, frame) {}

bool limitup_strategy::set_config(const std::map<std::string, std::string>& config)
{
	if (!strategy::set_config(config)) { return false; }
	auto contracts = trim(get_config("contracts"));
	if (contracts.empty())
	{
		contracts = trim(get_config("contract"));
	}
	if (contracts.empty())
	{
		printf("limitup config missing contract or contracts\n");
		return false;
	}
	_symbols = split_csv(contracts);
	if (_symbols.empty())
	{
		printf("limitup contracts parse failed\n");
		return false;
	}
	_symbol_set.clear();
	get_contracts().clear();
	for (const auto& s : _symbols)
	{
		_symbol_set.insert(s);
		get_contracts().insert(s);
	}
	return true;
}

void limitup_strategy::on_init()
{
	const auto market_mode = trim(get_config("mode_market"));
	const auto tempo_mode = trim(get_config("mode_tempo"));
	const auto exec_mode = trim(get_config("mode_exec"));
	const auto tick_epsilon = trim(get_config("tick_epsilon"));
	const auto min_seal_lot = trim(get_config("min_seal_lot"));
	const auto min_seal_amount = trim(get_config("min_seal_amount"));
	const auto reseal_window_ms = trim(get_config("reseal_window_ms"));
	const auto max_pullback_bp = trim(get_config("max_pullback_bp"));
	const auto signal_cooldown_sec = trim(get_config("signal_cooldown_sec"));
	const auto order_lot = trim(get_config("order_lot"));
	const auto max_order_lots = trim(get_config("max_order_lots"));
	const auto queue_timeout_ms = trim(get_config("queue_timeout_ms"));
	const auto max_capital_per_symbol = trim(get_config("max_capital_per_symbol"));
	const auto max_total_exposure = trim(get_config("max_total_exposure"));
	const auto max_signals_per_day = trim(get_config("max_signals_per_day"));
	const auto max_cancel_ratio = trim(get_config("max_cancel_ratio"));
	const auto no_new_position_after = trim(get_config("no_new_position_after"));

	try
	{
		if (!market_mode.empty()) { _market_mode = market_mode; }
		if (!tick_epsilon.empty()) { _tick_epsilon = std::stod(tick_epsilon); }
		if (!min_seal_lot.empty()) { _min_seal_lot = std::stoi(min_seal_lot); }
		if (!min_seal_amount.empty()) { _min_seal_amount = std::stod(min_seal_amount); }
		if (!reseal_window_ms.empty()) { _reseal_window_ms = std::stoll(reseal_window_ms); }
		if (!max_pullback_bp.empty()) { _max_pullback_bp = std::stod(max_pullback_bp); }
		if (!signal_cooldown_sec.empty()) { _signal_cooldown_sec = std::stoll(signal_cooldown_sec); }
		if (!order_lot.empty()) { _order_lot = std::stoi(order_lot); }
		if (!max_order_lots.empty()) { _max_order_lots = std::stoi(max_order_lots); }
		if (!queue_timeout_ms.empty()) { _queue_timeout_ms = std::stoll(queue_timeout_ms); }

		if (tempo_mode == "first_only") { _tempo_mode = tempo_mode::first_only; }
		else if (tempo_mode == "reseal_only") { _tempo_mode = tempo_mode::reseal_only; }
		else { _tempo_mode = tempo_mode::first_and_reseal; }

		if (exec_mode == "fill_first") { _exec_mode = exec_mode::fill_first; }
		else if (exec_mode == "risk_first") { _exec_mode = exec_mode::risk_first; }
		else { _exec_mode = exec_mode::queue_first; }

		risk_guard::config rcfg = _risk_guard.get_config();
		if (!max_capital_per_symbol.empty()) { rcfg.max_capital_per_symbol = std::stod(max_capital_per_symbol); }
		if (!max_total_exposure.empty()) { rcfg.max_total_exposure = std::stod(max_total_exposure); }
		if (!max_signals_per_day.empty()) { rcfg.max_signals_per_day = std::stoi(max_signals_per_day); }
		if (!max_cancel_ratio.empty()) { rcfg.cancel_cfg.max_cancel_ratio = std::stod(max_cancel_ratio); }
		if (!no_new_position_after.empty()) { rcfg.no_new_position_after_ms = parse_hms_ms(no_new_position_after.c_str(), 0); }
		_risk_guard.set_config(rcfg);

		position_sizer::config scfg;
		scfg.max_capital_per_symbol = rcfg.max_capital_per_symbol;
		scfg.lot_size = _order_lot;
		scfg.max_order_lots = _max_order_lots;
		_position_sizer.set_config(scfg);
	}
	catch (...)
	{
		printf("limitup config parse failed\n");
		return;
	}

	if (_order_lot <= 0 || _max_order_lots <= 0 || _min_seal_lot <= 0 || _signal_cooldown_sec < 0)
	{
		printf("limitup config value out of range\n");
		return;
	}
	_inited = true;
}

void limitup_strategy::on_tick(const MarketData& tick)
{
	if (!_inited) { return; }
	std::string symbol = tick.instrument_id;
	if (_symbol_set.find(symbol) == _symbol_set.end()) { return; }
	if (!is_market_allowed(symbol)) { return; }
	const int64_t ms_of_day = parse_hms_ms(tick.update_time, tick.update_millisec);
	if (!in_trade_window(ms_of_day)) { return; }

	auto& st = _states[symbol];
	st.day_high = std::max(st.day_high, tick.last_price);
	if (st.day_low <= 0) { st.day_low = tick.last_price; }
	else { st.day_low = std::min(st.day_low, tick.last_price); }

	if (st.cooldown_until_ms > ms_of_day) { return; }
	if (_active_order_by_symbol.find(symbol) != _active_order_by_symbol.end()) { return; }
	if (!_risk_guard.cancel_guard().allow_new_order(ms_of_day)) { return; }
	if (!_risk_guard.can_open(symbol, ms_of_day, _total_exposure, _symbol_exposure[symbol])) { return; }

	if (should_trigger(tick, st, ms_of_day))
	{
		trigger_buy(symbol, tick, ms_of_day);
	}
}

void limitup_strategy::on_tbt_entrust(const TickByTickEntrustData& entrust)
{
	if (_symbol_set.find(entrust.instrument_id) == _symbol_set.end()) { return; }
}

void limitup_strategy::on_tbt_trade(const TickByTickTradeData& trade)
{
	if (_symbol_set.find(trade.instrument_id) == _symbol_set.end()) { return; }
}

void limitup_strategy::on_order(const Order& order)
{
	_risk_guard.cancel_guard().on_order_event(parse_hms_ms(order.insert_time, 0), order.order_status == eOrderStatus::Canceled);
	if (order.order_status == eOrderStatus::Canceled)
	{
		clear_order(order.order_ref);
	}
}

void limitup_strategy::on_trade(const Order& order)
{
	const auto it = _pending_orders.find(order.order_ref);
	if (it != _pending_orders.end())
	{
		auto& st = _states[it->second.symbol];
		st.cooldown_until_ms = parse_hms_ms(order.insert_time, 0) + _signal_cooldown_sec * 1000;
		double trade_amount = order.limit_price * (order.volume_traded > 0 ? order.volume_traded : order.volume_total_original);
		_symbol_exposure[it->second.symbol] += trade_amount;
		_total_exposure += trade_amount;
		clear_order(order.order_ref);
	}
}

void limitup_strategy::on_cancel(const Order& order)
{
	clear_order(order.order_ref);
}

void limitup_strategy::on_error(const Order& order)
{
	clear_order(order.order_ref);
}

void limitup_strategy::on_update()
{
	if (_pending_orders.empty()) { return; }
	const auto now = std::chrono::steady_clock::now();
	std::vector<orderref_t> cancel_list;
	for (const auto& kv : _pending_orders)
	{
		const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - kv.second.submit_tp).count();
		if (elapsed >= _queue_timeout_ms)
		{
			cancel_list.push_back(kv.first);
		}
	}
	for (auto id : cancel_list)
	{
		cancel_order(id);
	}
}

std::string limitup_strategy::trim(const std::string& s)
{
	size_t l = 0;
	while (l < s.size() && std::isspace(static_cast<unsigned char>(s[l]))) { ++l; }
	size_t r = s.size();
	while (r > l && std::isspace(static_cast<unsigned char>(s[r - 1]))) { --r; }
	return s.substr(l, r - l);
}

std::vector<std::string> limitup_strategy::split_csv(const std::string& s)
{
	std::vector<std::string> out;
	size_t pos = 0;
	while (pos < s.size())
	{
		size_t next = s.find(',', pos);
		if (next == std::string::npos) { next = s.size(); }
		auto item = trim(s.substr(pos, next - pos));
		if (!item.empty()) { out.push_back(item); }
		pos = next + 1;
	}
	return out;
}

int64_t limitup_strategy::parse_hms_ms(const char* hhmmss, int millisec)
{
	if (!hhmmss || std::strlen(hhmmss) < 8) { return 0; }
	int hh = (hhmmss[0] - '0') * 10 + (hhmmss[1] - '0');
	int mm = (hhmmss[3] - '0') * 10 + (hhmmss[4] - '0');
	int ss = (hhmmss[6] - '0') * 10 + (hhmmss[7] - '0');
	return static_cast<int64_t>(((hh * 60 + mm) * 60 + ss) * 1000 + millisec);
}

bool limitup_strategy::is_symbol_mainboard(const std::string& symbol)
{
	if (symbol.size() < 3) { return false; }
	if (symbol.rfind("60", 0) == 0 || symbol.rfind("000", 0) == 0 || symbol.rfind("001", 0) == 0 || symbol.rfind("002", 0) == 0)
	{
		return true;
	}
	return false;
}

bool limitup_strategy::is_market_allowed(const std::string& symbol) const
{
	if (_market_mode == "all") { return true; }
	if (_market_mode == "mainboard") { return is_symbol_mainboard(symbol); }
	return is_symbol_mainboard(symbol);
}

bool limitup_strategy::in_trade_window(int64_t ms_of_day) const
{
	const int64_t m0930 = (9 * 3600 + 30 * 60) * 1000;
	const int64_t m1130 = (11 * 3600 + 30 * 60) * 1000;
	const int64_t m1300 = (13 * 3600) * 1000;
	const int64_t m1450 = (14 * 3600 + 50 * 60) * 1000;
	return (m0930 <= ms_of_day && ms_of_day <= m1130) || (m1300 <= ms_of_day && ms_of_day <= m1450);
}

bool limitup_strategy::should_trigger(const MarketData& tick, symbol_state& st, int64_t ms_of_day)
{
	const double upper = tick.upper_limit_price;
	if (upper <= 0) { return false; }
	const bool is_limit = tick.last_price >= upper - _tick_epsilon;
	const bool is_sealed = tick.bid_price[0] >= upper - _tick_epsilon &&
		tick.bid_volume[0] >= _min_seal_lot &&
		(_min_seal_amount <= 0 || tick.bid_price[0] * tick.bid_volume[0] >= _min_seal_amount);

	if (st.ever_sealed)
	{
		if (tick.last_price < upper - _tick_epsilon || tick.ask_price[0] < upper - _tick_epsilon)
		{
			st.opened_since_seal = true;
			st.opened_ms = ms_of_day;
		}
	}

	if (!(is_limit && is_sealed)) { return false; }
	st.ever_sealed = true;

	if (_tempo_mode == tempo_mode::first_only)
	{
		return !st.first_triggered;
	}
	if (_tempo_mode == tempo_mode::reseal_only)
	{
		return st.opened_since_seal && (ms_of_day - st.opened_ms <= _reseal_window_ms);
	}
	if (!st.first_triggered) { return true; }
	return st.opened_since_seal && (ms_of_day - st.opened_ms <= _reseal_window_ms);
}

void limitup_strategy::trigger_buy(const std::string& symbol, const MarketData& tick, int64_t ms_of_day)
{
	const int volume = _position_sizer.calc_order_volume(_risk_guard.get_config().max_total_exposure - _total_exposure, tick.upper_limit_price, _symbol_exposure[symbol]);
	if (volume <= 0) { return; }
	double price = tick.upper_limit_price;
	if (_exec_mode == exec_mode::fill_first)
	{
		price = std::max(tick.ask_price[0], tick.upper_limit_price);
	}
	auto order_ref = buy_open(eOrderFlag::Limit, symbol, price, volume);
	if (order_ref == null_orderref) { return; }
	_pending_orders[order_ref] = pending_order{ symbol, std::chrono::steady_clock::now() };
	_active_order_by_symbol[symbol] = order_ref;
	_states[symbol].first_triggered = true;
	_last_signal_ms[symbol] = ms_of_day;
	_risk_guard.on_signal();
}

void limitup_strategy::clear_order(orderref_t order_ref)
{
	auto it = _pending_orders.find(order_ref);
	if (it == _pending_orders.end()) { return; }
	_active_order_by_symbol.erase(it->second.symbol);
	_pending_orders.erase(it);
}
