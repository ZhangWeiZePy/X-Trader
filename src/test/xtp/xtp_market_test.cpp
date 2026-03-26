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

	rt.start_service();
	std::cout << "Market service started. Waiting for ticks..." << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(20));
	std::cout << "Market test finished." << std::endl;
	rt.stop_service();
	rt.release();

	return 0;
}
