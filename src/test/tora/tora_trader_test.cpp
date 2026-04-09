#include "interface.h"
#include "INIReader.h"
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>

static bool load_trader_config(const std::string &ini_file, std::map<std::string, std::string> &td_config)
{
    INIReader reader(ini_file);
    if (reader.ParseError() < 0)
    {
        std::cout << "can't load " << ini_file << std::endl;
        return false;
    }

    td_config["trade_front"] = reader.Get("trader", "trade_front", "");
    td_config["broker_id"] = reader.Get("trader", "broker_id", "");
    td_config["user_id"] = reader.Get("trader", "user_id", "");
    td_config["password"] = reader.Get("trader", "password", "");
    td_config["app_id"] = reader.Get("trader", "app_id", "");
    td_config["auth_code"] = reader.Get("trader", "auth_code", "");
    td_config["software_key"] = reader.Get("trader", "software_key", "");
    td_config["user_product_Info"] = reader.Get("trader", "user_product_Info", "");
    td_config["client_id"] = reader.Get("trader", "client_id", "");
    td_config["sock_type"] = reader.Get("trader", "sock_type", "");
    td_config["counter"] = reader.Get("trader", "counter", "");
    return true;
}

static bool wait_trader_ready(trader_api *trader, int timeout_ms)
{
    const auto start = std::chrono::steady_clock::now();
    while (!trader->_is_ready.load(std::memory_order_acquire))
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return true;
}

static void print_positions(const PositionMap &p_map)
{
    std::cout << "持仓条数: " << p_map.size() << std::endl;
    for (const auto &[contract, pos]: p_map)
    {
        std::cout << "  合约=" << contract
                << " 多头持仓=" << pos.long_.position
                << " 多头可卖=" << pos.long_.closeable
                << " 空头持仓=" << pos.short_.position
                << " 空头可买=" << pos.short_.closeable
                << std::endl;
    }
}

static void print_orders(const OrderMap &o_map)
{
    std::cout << "委托条数: " << o_map.size() << std::endl;
    for (const auto &[order_ref, order]: o_map)
    {
        std::cout << "  order_ref=" << order_ref
                << " 合约=" << order.instrument_id
                << " 价格=" << order.limit_price
                << " 原始数量=" << order.volume_total_original
                << " 已成交=" << order.volume_traded
                << " 剩余=" << order.volume_total
                << " 状态=" << geteOrderStatusString(order.order_status)
                << std::endl;
    }
}

int main()
{
    const std::string ini_file = "./ini/tora/test.ini";
    const std::set<std::string> contracts = {"600850"};
    std::map<std::string, std::string> td_config;

    if (!load_trader_config(ini_file, td_config))
    {
        return -1;
    }

    trader_api *trader = nullptr;
    try
    {
        trader = create_trader(td_config, contracts);
    } catch (const std::exception &ex)
    {
        std::cout << "创建交易接口失败: " << ex.what() << std::endl;
        return -2;
    }

    if (!wait_trader_ready(trader, 10000))
    {
        std::cout << "交易接口未在超时内就绪" << std::endl;
        destory_trader(trader);
        return -3;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    InstrumentMap i_map;
    PositionMap p_map;
    OrderMap o_map;

    std::cout << "[测试A] 查询持仓" << std::endl;
    trader->get_trader_data(i_map, p_map, o_map);
    print_positions(p_map);

    std::cout << "[测试B] 下单一手 600850" << std::endl;
    orderref_t order_ref = trader->insert_order(eOrderFlag::Market, "600850", eDirOffset::BuyOpen, 0.0, 100);
    if (order_ref == null_orderref)
    {
        std::cout << "下单失败，返回 null_orderref" << std::endl;
    } else
    {
        std::cout << "下单请求已发送，order_ref=" << order_ref << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "[测试C] 查询委托" << std::endl;
    trader->get_trader_data(i_map, p_map, o_map);
    print_orders(o_map);

    trader->release();
    destory_trader(trader);
    return 0;
}
