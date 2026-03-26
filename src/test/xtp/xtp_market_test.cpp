#include "realtime.h"
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <chrono>

int main()
{
	std::string ini_file = "./ini/xtp/test.ini";
	std::set<std::string> contracts = { "600850" };

	realtime rt;
	if (!rt.init(ini_file, contracts))
	{
		std::cout << "Failed to init realtime with " << ini_file << std::endl;
		return -1;
	}

	rt.bind_tick_event([](const MarketData& tick) {
		std::cout << std::fixed << std::setprecision(4);
		auto print_row = [](const char* key, const auto& value) {
			std::cout << "| " << std::left << std::setw(20) << key
				<< " | " << std::right << std::setw(28) << value << " |\n";
		};
		std::cout << "\n+----------------------+------------------------------+\n";
		std::cout << "| Field                | Value                        |\n";
		std::cout << "+----------------------+------------------------------+\n";
		print_row("instrument_id", tick.instrument_id);
		print_row("update_time", tick.update_time);
		print_row("update_millisec", tick.update_millisec);
		print_row("pre_close_price", tick.pre_close_price);
		print_row("pre_settlement_price", tick.pre_settlement_price);
		print_row("last_price", tick.last_price);
		print_row("volume", tick.volume);
		print_row("last_volume", tick.last_volume);
		print_row("open_interest", tick.open_interest);
		print_row("last_open_interest", tick.last_open_interest);
		print_row("open_price", tick.open_price);
		print_row("highest_price", tick.highest_price);
		print_row("lowest_price", tick.lowest_price);
		print_row("upper_limit_price", tick.upper_limit_price);
		print_row("lower_limit_price", tick.lower_limit_price);
		print_row("settlement_price", tick.settlement_price);
		print_row("average_price", tick.average_price);
		print_row("tape_dir", static_cast<int>(tick.tape_dir));
		std::cout << "+----------------------+------------------------------+\n";

		std::cout << "+-------+---------------+------------+---------------+------------+\n";
		std::cout << "| Level | BidPrice      | BidVolume  | AskPrice      | AskVolume  |\n";
		std::cout << "+-------+---------------+------------+---------------+------------+\n";
		for (int i = 0; i < 10; ++i)
		{
			std::cout << "| " << std::left << std::setw(5) << (i + 1)
				<< " | " << std::right << std::setw(13) << tick.bid_price[i]
				<< " | " << std::setw(10) << tick.bid_volume[i]
				<< " | " << std::setw(13) << tick.ask_price[i]
				<< " | " << std::setw(10) << tick.ask_volume[i]
				<< " |\n";
		}
		std::cout << "+-------+---------------+------------+---------------+------------+\n";
	});

	// 逐笔委托打印：用于观察逐笔委托回报字段
	rt.get_market().bind_tbt_entrust_callback([](const TickByTickEntrustData& entrust) {
		std::cout << "[TBT_ENTRUST]"
			<< " instrument_id=" << entrust.instrument_id
			<< " update_time=" << entrust.update_time
			<< "." << entrust.update_millisec
			<< " channel_no=" << entrust.channel_no
			<< " seq=" << entrust.seq
			<< " price=" << entrust.price
			<< " qty=" << entrust.qty
			<< " side=" << entrust.side
			<< " ord_type=" << entrust.ord_type
			<< " order_no=" << entrust.order_no
			<< std::endl;
	});

	// 逐笔成交打印：按单行输出
	rt.get_market().bind_tbt_trade_callback([](const TickByTickTradeData& trade) {
		std::cout << "[TBT_TRADE]"
			<< " instrument_id=" << trade.instrument_id
			<< " update_time=" << trade.update_time
			<< "." << trade.update_millisec
			<< " channel_no=" << trade.channel_no
			<< " seq=" << trade.seq
			<< " price=" << trade.price
			<< " qty=" << trade.qty
			<< " money=" << trade.money
			<< " bid_no=" << trade.bid_no
			<< " ask_no=" << trade.ask_no
			<< " trade_flag=" << trade.trade_flag
			<< std::endl;
	});

	rt.start_service();
	std::cout << "Market service started. Waiting for ticks..." << std::endl;
	rt.stop_service();
	rt.release();
	return 0;
}
