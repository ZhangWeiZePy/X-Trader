#include <cassert>
#include <fstream>
#include "tick_reader.h"
#include "sim_trader.h"

int main()
{
	const std::string path = "limitup_replay_test.csv";
	{
		std::ofstream ofs(path);
		ofs << "20260101093000,600000,09:30:00,0,10.00,10.00,10.00,500000,10.01,1000\n";
		ofs << "20260101093001,600000,09:30:01,0,10.00,10.00,10.00,500000,10.00,2000\n";
	}

	tick_reader reader;
	assert(reader.open(path));
	MarketData tick{};
	assert(reader.read_next(tick));
	assert(std::string(tick.instrument_id) == "600000");
	assert(tick.upper_limit_price == 10.0);

	sim_trader trader;
	int trade_count = 0;
	trader.set_event_callback([&trade_count](const Order& order)->void {
		if (order.event_flag == eEventFlag::Trade) { ++trade_count; }
		});
	const auto id = trader.submit_buy_limit("600000", 10.0, 1000);
	assert(id != null_orderref);
	trader.on_tick(tick);
	assert(trade_count == 0);
	assert(reader.read_next(tick));
	trader.on_tick(tick);
	assert(trade_count == 1);
	return 0;
}
