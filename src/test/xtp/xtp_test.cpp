#include "realtime.h"
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>

static std::atomic<double> g_latest_ask_price{0.0};

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
		double ask1 = tick.ask_price[0] > 0 ? tick.ask_price[0] : tick.last_price;
		if (ask1 > 0) {
			g_latest_ask_price.store(ask1, std::memory_order_release);
		}
		std::cout << "[Tick] Instrument: " << tick.instrument_id
				  << " LastPrice: " << tick.last_price
				  << " Volume: " << tick.volume
				  << " Bid1: " << tick.bid_price[0] << " (" << tick.bid_volume[0] << ")"
				  << " Ask1: " << tick.ask_price[0] << " (" << tick.ask_volume[0] << ")"
				  << std::endl;
	});

	OrderEvent order_event;
	order_event.on_order = [](const Order& order) {
		std::cout << "[Order] " << order.instrument_id << " Ref: " << order.order_ref << " Status: " << geteOrderStatusString(order.order_status) << std::endl;
	};
	order_event.on_trade = [](const Order& order) {
		std::cout << "[Trade] " << order.instrument_id << " Ref: " << order.order_ref << " Traded: " << order.volume_traded << std::endl;
	};
	order_event.on_cancel = [](const Order& order) {
		std::cout << "[Cancel] " << order.instrument_id << " Ref: " << order.order_ref << std::endl;
	};
	order_event.on_error = [](const Order& order) {
		std::cout << "[Error] " << order.instrument_id << " Ref: " << order.order_ref << " ErrorMsg: " << order.error_msg << std::endl;
	};

	rt.bind_order_event(order_event);

	rt.start_service();
	std::cout << "Service started. Wait for ready..." << std::endl;

	std::this_thread::sleep_for(std::chrono::seconds(5));

	// a. 查询持仓
	std::cout << "\n--- a. Query Position ---" << std::endl;
	const Position& p = rt.get_position("600850");
	rt.print_position(p, "Initial");

	// b. 现价购买1手600850
	std::cout << "\n--- b. Buy 1 lot 600850 at market price ---" << std::endl;
	double market_protect_price = g_latest_ask_price.load(std::memory_order_acquire);
	if (market_protect_price <= 0.0) {
		market_protect_price = 23.0;
	}
	orderref_t order1 = rt.insert_order(eOrderFlag::Market, "600850", eDirOffset::BuyOpen, market_protect_price, 100);
	std::cout << "Order 1 Ref: " << order1 << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// c. 21元购买1手600850
	std::cout << "\n--- c. Buy 1 lot 600850 at 21.0 (limit) ---" << std::endl;
	orderref_t order2 = rt.insert_order(eOrderFlag::Limit, "600850", eDirOffset::BuyOpen, 21.0, 100);
	std::cout << "Order 2 Ref: " << order2 << std::endl;
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// d. 查询委托
	std::cout << "\n--- d. Query Order ---" << std::endl;
	const Order& o2 = rt.get_order(order2);
	rt.print_order(o2);

	// e. 撤回c步骤下的单
	std::cout << "\n--- e. Cancel Order 2 ---" << std::endl;
	const Order& order2_state = rt.get_order(order2);
	if (order2_state.order_status == eOrderStatus::AllTraded || order2_state.order_status == eOrderStatus::Canceled) {
		std::cout << "Cancel skipped. Current status: " << geteOrderStatusString(order2_state.order_status) << std::endl;
	}
	else {
		bool cancel_res = rt.cancel_order(order2);
		std::cout << "Cancel Result: " << cancel_res << std::endl;
	}
	std::this_thread::sleep_for(std::chrono::seconds(3));

	// f. 查询持仓
	std::cout << "\n--- f. Query Position ---" << std::endl;
	const Position& p2 = rt.get_position("600850");
	rt.print_position(p2, "Final");

	std::cout << "Test Finished." << std::endl;
	rt.stop_service();
	rt.release();

	return 0;
}
