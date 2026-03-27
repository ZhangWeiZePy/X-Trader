#include "position_sizer.h"

#include <algorithm>

int position_sizer::calc_order_volume(double cash_available, double price, double symbol_used_capital) const
{
	if (price <= 0) { return 0; }
	const double symbol_left_cap = std::max(0.0, _cfg.max_capital_per_symbol - symbol_used_capital);
	const double usable_cash = std::min(cash_available, symbol_left_cap);
	if (usable_cash <= 0) { return 0; }
	const int max_volume_by_cash = static_cast<int>(usable_cash / price);
	const int max_volume_by_lots = _cfg.lot_size * _cfg.max_order_lots;
	const int volume = std::min(max_volume_by_cash, max_volume_by_lots);
	return volume / _cfg.lot_size * _cfg.lot_size;
}
