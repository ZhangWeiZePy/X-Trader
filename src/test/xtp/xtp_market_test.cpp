#include "interface.h"
#include "INIReader.h"
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <map>
#include <cstdio>

static void print_usage()
{
    std::cout << "xtp_market_test <command> [options]\n\n";
    std::cout << "Commands:\n";
    std::cout << "  tick                    打印 tick 行情\n";
    std::cout << "  tbt_entrust             打印逐笔委托\n";
    std::cout << "  tbt_trade               打印逐笔成交\n\n";
    std::cout << "  orderbook               打印本地重建订单薄\n\n";
    std::cout << "Options:\n";
    std::cout << "  --ini <path>             ini 路径，默认 ./ini/xtp/test.ini\n";
    std::cout << "  --contracts <codes>      合约列表，逗号分隔，默认 600850\n";
    std::cout << "  --timeout <ms>           等待行情就绪超时，默认 10000\n";
    std::cout << "  --seconds <n>            运行时长，默认 10\n";
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

static std::set<std::string> parse_contracts(const std::string &text)
{
    std::set<std::string> result;
    std::string buf;
    for (char ch: text)
    {
        if (ch == ',')
        {
            if (!buf.empty())
            {
                result.insert(buf);
                buf.clear();
            }
            continue;
        }
        if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
        {
            buf.push_back(ch);
        }
    }
    if (!buf.empty())
    {
        result.insert(buf);
    }
    return result;
}

static bool load_market_config(const std::string &ini_file, std::map<std::string, std::string> &md_config)
{
    INIReader reader(ini_file);
    if (reader.ParseError() < 0)
    {
        std::cout << "can't load " << ini_file << std::endl;
        return false;
    }

    md_config["market_front"] = reader.Get("market", "market_front", "");
    md_config["broker_id"] = reader.Get("market", "broker_id", "");
    md_config["user_id"] = reader.Get("market", "user_id", "");
    md_config["password"] = reader.Get("market", "password", "");
    md_config["counter"] = reader.Get("market", "counter", "");
    return true;
}

static bool wait_market_ready(market_api *market, int timeout_ms)
{
    const auto start = std::chrono::steady_clock::now();
    while (!market->_is_ready.load(std::memory_order_acquire))
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
        if (elapsed >= timeout_ms)
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return true;
}

static std::string format_xtp_data_time(int64_t data_time)
{
    if (data_time <= 0)
    {
        return "N/A";
    }
    long long t = data_time;
    const int ms = static_cast<int>(t % 1000);
    t /= 1000;
    const int ss = static_cast<int>(t % 100);
    t /= 100;
    const int mm = static_cast<int>(t % 100);
    t /= 100;
    const int hh = static_cast<int>(t % 100);
    t /= 100;
    const int dd = static_cast<int>(t % 100);
    t /= 100;
    const int mon = static_cast<int>(t % 100);
    t /= 100;
    const int yyyy = static_cast<int>(t % 10000);

    char buf[32] = {0};
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  yyyy, mon, dd, hh, mm, ss, ms);
    return std::string(buf);
}

static void bind_tick_printer(market_api &market)
{
    market.bind_callback([](const MarketData &tick)
    {
        std::cout << std::fixed << std::setprecision(4);
        auto print_row = [](const char *key, const auto &value)
        {
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
}

static void bind_tick_noop(market_api &market)
{
    market.bind_callback([](const MarketData &)
            {});
}

static void bind_tbt_entrust_printer(market_api &market)
{
    market.bind_tbt_entrust_callback([](const TickByTickEntrustData &entrust)
    {
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
}

static void bind_tbt_trade_printer(market_api &market)
{
    market.bind_tbt_trade_callback([](const TickByTickTradeData &trade)
    {
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
}

static void bind_orderbook_printer(market_api &market)
{
    market.bind_orderbook_callback([](const OrderBookData &ob)
    {
        std::cout << std::fixed << std::setprecision(4);
        auto print_row = [](const char *key, const auto &value)
        {
            std::cout << "| " << std::left << std::setw(20) << key
                    << " | " << std::right << std::setw(28) << value << " |\n";
        };
        std::cout << "\n+----------------------+------------------------------+\n";
        std::cout << "| Field                | Value                        |\n";
        std::cout << "+----------------------+------------------------------+\n";
        print_row("exchange_id", ob.exchange_id);
        print_row("instrument_id", ob.instrument_id);
        print_row("last_price", ob.last_price);
        print_row("qty", ob.qty);
        print_row("turnover", ob.turnover);
        print_row("trades_count", ob.trades_count);
        print_row("data_time", format_xtp_data_time(ob.data_time));
        std::cout << "+----------------------+------------------------------+\n";

        std::cout << "+-------+---------------+------------+---------------+------------+\n";
        std::cout << "| Level | BidPrice      | BidVolume  | AskPrice      | AskVolume  |\n";
        std::cout << "+-------+---------------+------------+---------------+------------+\n";
        for (int i = 0; i < 10; ++i)
        {
            std::cout << "| " << std::left << std::setw(5) << (i + 1)
                    << " | " << std::right << std::setw(13) << ob.bid[i]
                    << " | " << std::setw(10) << ob.bid_qty[i]
                    << " | " << std::setw(13) << ob.ask[i]
                    << " | " << std::setw(10) << ob.ask_qty[i]
                    << " |\n";
        }
        std::cout << "+-------+---------------+------------+---------------+------------+\n";
    });
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
    const std::string contracts_text = get_opt(args, "--contracts", "600850");
    const std::set<std::string> contracts = parse_contracts(contracts_text);
    const int timeout_ms = std::stoi(get_opt(args, "--timeout", "10000"));
    const int seconds = std::stoi(get_opt(args, "--seconds", "10"));
    if (contracts.empty())
    {
        std::cout << "合约列表为空" << std::endl;
        return -1;
    }

    std::map<std::string, std::string> md_config;
    if (!load_market_config(ini_file, md_config))
    {
        return -1;
    }

    market_api *market = create_market(md_config, contracts);
    if (!market)
    {
        std::cout << "创建行情接口失败" << std::endl;
        return -2;
    }

    if (command == "tick")
    {
        bind_tick_printer(*market);
    } else if (command == "tbt_entrust")
    {
        bind_tick_noop(*market);
        bind_tbt_entrust_printer(*market);
    } else if (command == "tbt_trade")
    {
        bind_tick_noop(*market);
        bind_tbt_trade_printer(*market);
    } else if (command == "orderbook")
    {
        bind_tick_noop(*market);
        bind_orderbook_printer(*market);
    } else
    {
        print_usage();
        destory_market(market);
        return -1;
    }

    if (!wait_market_ready(market, timeout_ms))
    {
        std::cout << "行情接口未在超时内就绪" << std::endl;
        market->release();
        destory_market(market);
        return -3;
    }

    std::cout << "Market service started. Waiting for events..." << std::endl;
    const auto end_time = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < end_time)
    {
        market->process_event();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    market->release();
    destory_market(market);
    return 0;
}
