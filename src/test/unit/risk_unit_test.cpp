#include <cassert>
#include "risk/position_sizer.h"
#include "risk/cancel_rate_guard.h"
#include "risk/risk_guard.h"

int main()
{
	position_sizer::config scfg;
	scfg.max_capital_per_symbol = 100000;
	scfg.lot_size = 100;
	scfg.max_order_lots = 5;
	position_sizer sizer(scfg);
	const int vol = sizer.calc_order_volume(200000, 10.0, 0);
	assert(vol == 500);

	cancel_rate_guard::config ccfg;
	ccfg.max_cancel_ratio = 0.5;
	ccfg.min_order_count = 2;
	cancel_rate_guard cguard(ccfg);
	cguard.on_order_event(1000, false);
	cguard.on_order_event(1100, true);
	assert(cguard.allow_new_order(1200));
	cguard.on_order_event(1300, true);
	assert(!cguard.allow_new_order(1400));

	risk_guard::config rcfg;
	rcfg.max_total_exposure = 1000000;
	rcfg.max_capital_per_symbol = 100000;
	rcfg.max_signals_per_day = 2;
	risk_guard rguard(rcfg);
	assert(rguard.can_open("600000", 1000, 0, 0));
	rguard.on_signal();
	rguard.on_signal();
	assert(rguard.reach_signal_limit());
	assert(!rguard.can_open("600000", 1000, 0, 0));
	return 0;
}
