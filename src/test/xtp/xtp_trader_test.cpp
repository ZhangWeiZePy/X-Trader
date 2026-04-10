#include "interface.h"
#include "INIReader.h"
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

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

static void print_usage()
{
    std::cout << "xtp_trader_test <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  position                 持仓查询\n";
    std::cout << "  order                    委托查询\n";
    std::cout << "  insert                   下单功能\n";
    std::cout << "  cancel                   撤单功能\n\n";
    std::cout << "Common options:\n";
    std::cout << "  --ini <path>             ini 路径，默认 ./ini/xtp/test.ini\n";
    std::cout << "  --timeout <ms>           等待就绪超时，默认 10000\n";
    std::cout << "  --contract <code>        合约代码（可选，默认 600850，用于订阅集合）\n\n";
    std::cout << "Insert options:\n";
    std::cout << "  --dir <BuyOpen|SellOpen|BuyClose|SellClose|BuyCloseToday|SellCloseToday|BuyCloseYesterday|SellCloseYesterday>\n";
    std::cout << "  --flag <Limit|Market|FOK|FAK>\n";
    std::cout << "  --price <double>\n";
    std::cout << "  --volume <int>\n\n";
    std::cout << "Cancel options:\n";
    std::cout << "  --order_ref <uint64>\n";
}

static std::string get_opt(const std::vector<std::string> &args, const std::string &key, const std::string &def = "")
{
    for (size_t i = 0; i + 1 < args.size(); i++)
    {
        if (args[i] == key)
        {
            return args[i + 1];
        }
    }
    return def;
}

static bool has_opt(const std::vector<std::string> &args, const std::string &key)
{
    for (const auto &a: args)
    {
        if (a == key)
        {
            return true;
        }
    }
    return false;
}

static bool parse_dir(const std::string &text, eDirOffset &out)
{
    if (text == "BuyOpen")
    {
        out = eDirOffset::BuyOpen;
        return true;
    }
    if (text == "SellOpen")
    {
        out = eDirOffset::SellOpen;
        return true;
    }
    if (text == "BuyClose")
    {
        out = eDirOffset::BuyClose;
        return true;
    }
    if (text == "SellClose")
    {
        out = eDirOffset::SellClose;
        return true;
    }
    if (text == "BuyCloseToday")
    {
        out = eDirOffset::BuyCloseToday;
        return true;
    }
    if (text == "SellCloseToday")
    {
        out = eDirOffset::SellCloseToday;
        return true;
    }
    if (text == "BuyCloseYesterday")
    {
        out = eDirOffset::BuyCloseYesterday;
        return true;
    }
    if (text == "SellCloseYesterday")
    {
        out = eDirOffset::SellCloseYesterday;
        return true;
    }
    return false;
}

static bool parse_flag(const std::string &text, eOrderFlag &out)
{
    if (text == "Limit")
    {
        out = eOrderFlag::Limit;
        return true;
    }
    if (text == "Market")
    {
        out = eOrderFlag::Market;
        return true;
    }
    if (text == "FOK")
    {
        out = eOrderFlag::FOK;
        return true;
    }
    if (text == "FAK")
    {
        out = eOrderFlag::FAK;
        return true;
    }
    return false;
}

static trader_api *create_and_wait(const std::string &ini_file, const std::string &contract, int timeout_ms)
{
    std::map<std::string, std::string> td_config;
    if (!load_trader_config(ini_file, td_config))
    {
        return nullptr;
    }

    std::set<std::string> contracts;
    if (!contract.empty())
    {
        contracts.insert(contract);
    }

    trader_api *trader = nullptr;
    try
    {
        trader = create_trader(td_config, contracts);
    } catch (const std::exception &ex)
    {
        std::cout << "创建交易接口失败: " << ex.what() << std::endl;
        return nullptr;
    }

    if (!wait_trader_ready(trader, timeout_ms))
    {
        std::cout << "交易接口未在超时内就绪" << std::endl;
        destory_trader(trader);
        return nullptr;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));
    return trader;
}

int main(int argc, char **argv)
{
    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(argc));
    for (int i = 0; i < argc; i++)
    {
        args.emplace_back(argv[i]);
    }

    if (argc < 2 || has_opt(args, "--help") || has_opt(args, "-h"))
    {
        print_usage();
        return 0;
    }

    const std::string command = args[1];
    const std::string ini_file = get_opt(args, "--ini", "./ini/xtp/test.ini");
    const std::string contract = get_opt(args, "--contract", "600850");
    const int timeout_ms = std::stoi(get_opt(args, "--timeout", "10000"));

    if (command == "position")
    {
        trader_api *trader = create_and_wait(ini_file, contract, timeout_ms);
        if (!trader)
        {
            return -2;
        }

        InstrumentMap i_map;
        PositionMap p_map;
        OrderMap o_map;
        trader->get_trader_data(i_map, p_map, o_map);
        print_positions(p_map);
        trader->release();
        destory_trader(trader);
        return 0;
    }

    if (command == "order")
    {
        trader_api *trader = create_and_wait(ini_file, contract, timeout_ms);
        if (!trader)
        {
            return -2;
        }

        InstrumentMap i_map;
        PositionMap p_map;
        OrderMap o_map;
        trader->get_trader_data(i_map, p_map, o_map);
        print_orders(o_map);
        trader->release();
        destory_trader(trader);
        return 0;
    }

    if (command == "insert")
    {
        eDirOffset dir{};
        eOrderFlag flag{};

        const std::string dir_text = get_opt(args, "--dir");
        const std::string flag_text = get_opt(args, "--flag", "Limit");
        const std::string contract_code = get_opt(args, "--contract", "600850");
        const double price = std::stod(get_opt(args, "--price", "0"));
        const int volume = std::stoi(get_opt(args, "--volume", "0"));

        if (!parse_dir(dir_text, dir) || !parse_flag(flag_text, flag) || contract_code.empty() || price <= 0 ||
            volume <= 0)
        {
            print_usage();
            return -1;
        }

        trader_api *trader = create_and_wait(ini_file, contract_code, timeout_ms);
        if (!trader)
        {
            return -2;
        }

        const orderref_t order_ref = trader->insert_order(flag, contract_code, dir, price, volume);
        if (order_ref == null_orderref)
        {
            std::cout << "下单失败，返回 null_orderref" << std::endl;
        } else
        {
            std::cout << "下单请求已发送，order_ref=" << order_ref << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(2));
        InstrumentMap i_map;
        PositionMap p_map;
        OrderMap o_map;
        trader->get_trader_data(i_map, p_map, o_map);
        print_orders(o_map);

        trader->release();
        destory_trader(trader);
        return 0;
    }

    if (command == "cancel")
    {
        const std::string ref_text = get_opt(args, "--order_ref");
        if (ref_text.empty())
        {
            print_usage();
            return -1;
        }

        const orderref_t order_ref = static_cast<orderref_t>(std::strtoull(ref_text.c_str(), nullptr, 10));
        if (order_ref == null_orderref)
        {
            std::cout << "order_ref 无效" << std::endl;
            return -1;
        }

        trader_api *trader = create_and_wait(ini_file, contract, timeout_ms);
        if (!trader)
        {
            return -2;
        }

        const bool ok = trader->cancel_order(order_ref);
        std::cout << "撤单请求发送结果: " << (ok ? "true" : "false") << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(2));
        InstrumentMap i_map;
        PositionMap p_map;
        OrderMap o_map;
        trader->get_trader_data(i_map, p_map, o_map);
        print_orders(o_map);

        trader->release();
        destory_trader(trader);
        return 0;
    }

    print_usage();
    return -1;
}
