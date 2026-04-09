#include "xtp_market.h"
#include <iostream>
#include <filesystem>
#include <cstring>

xtp_market::xtp_market(std::map<std::string, std::string> &config, std::set<std::string> &contracts) :
    _contracts(contracts), _md_api(nullptr)
{
    if (config.find("counter") == config.end() || config["counter"].empty())
    {
        throw std::runtime_error("Missing or empty 'counter' in config");
    }
    std::string counter = config["counter"];
#ifdef _WIN32
	std::string lib_path = "lib/" + counter + "/xtpquoteapi.dll";
	const char* creator_name = "?CreateQuoteApi@QuoteApi@API@XTP@@SAPEAV123@EPEBDW4XTP_LOG_LEVEL@@@Z";
#else
    std::string lib_path = "lib/" + counter + "/libxtpquoteapi.so";
    const char *creator_name = "_ZN3XTP3API8QuoteApi14CreateQuoteApiEhPKc13XTP_LOG_LEVEL";
#endif
    if (!_loader.load(lib_path))
    {
        throw std::runtime_error("Failed to load library: " + lib_path + ", error: " + _loader.get_error());
    }
    typedef XTP::API::QuoteApi * (*CreateQuoteApi_t)(uint8_t, const char *, XTP_LOG_LEVEL);
    CreateQuoteApi_t creator = _loader.get_function<CreateQuoteApi_t>(creator_name);
    if (!creator)
    {
        throw std::runtime_error("Failed to find symbol CreateQuoteApi in " + lib_path);
    }
    _user_id = config["user_id"];
    _password = config["password"];
    _client_id = config.count("client_id") ? std::stoi(config["client_id"]) : 1;
    std::string front = config["market_front"]; // format like "tcp://127.0.0.1:8000" or "127.0.0.1:8000"
    size_t pos = front.find("://");
    if (pos != std::string::npos)
    {
        front = front.substr(pos + 3);
    }
    pos = front.find(":");
    if (pos != std::string::npos)
    {
        _server_ip = front.substr(0, pos);
        _server_port = std::stoi(front.substr(pos + 1));
    } else
    {
        _server_ip = front;
        _server_port = 0;
    }

    int sock_type = config.count("sock_type") ? std::stoi(config["sock_type"]) : 1;
    _protocol_type = (sock_type == 2) ? XTP_PROTOCOL_UDP : XTP_PROTOCOL_TCP;

    char flow_path[64]{};
    sprintf(flow_path, "flow/xtp/md/%s/", _user_id.c_str());
    if (!std::filesystem::exists(flow_path))
    {
        std::filesystem::create_directories(flow_path);
    }

    _md_api = creator(_client_id, flow_path, XTP_LOG_LEVEL_INFO);
    if (!_md_api)
    {
        throw std::runtime_error("Failed to create XTP Quote API");
    }

    _md_api->SetHeartBeatInterval(15);
    _md_api->RegisterSpi(this);

    int ret = _md_api->Login(_server_ip.c_str(), _server_port, _user_id.c_str(), _password.c_str(), _protocol_type);
    if (ret != 0)
    {
        XTPRI *error_info = _md_api->GetApiLastError();
        std::cout << "xtp md login error: " << (error_info ? error_info->error_msg : "unknown") << std::endl;
    } else
    {
        std::cout << "xtp md login success!" << std::endl;
        _is_ready.store(true, std::memory_order_release);
        subscribe();
    }
}

xtp_market::~xtp_market()
{
    release();
}

void xtp_market::release()
{
    _is_ready.store(false);
    if (_md_api)
    {
        _md_api->Logout();
        _md_api->RegisterSpi(nullptr);
        _md_api->Release();
        _md_api = nullptr;
    }
}

void xtp_market::subscribe()
{
    if (_contracts.empty())
        return;

    std::vector<std::string> sh_contracts;
    std::vector<std::string> sz_contracts;

    for (const auto &contract: _contracts)
    {
        if (contract.length() > 0 && (contract[0] == '6' || contract[0] == '5'))
        {
            sh_contracts.push_back(contract);
        } else
        {
            sz_contracts.push_back(contract);
        }
    }

    if (!sh_contracts.empty())
    {
        char **ppInsts = new char *[sh_contracts.size()];
        for (size_t i = 0; i < sh_contracts.size(); ++i)
        {
            ppInsts[i] = new char[XTP_TICKER_LEN];
            strcpy(ppInsts[i], sh_contracts[i].c_str());
        }
        _md_api->SubscribeMarketData(ppInsts, sh_contracts.size(), XTP_EXCHANGE_SH);
        // 同时订阅上交所逐笔（委托/成交）
        _md_api->SubscribeTickByTick(ppInsts, sh_contracts.size(), XTP_EXCHANGE_SH);
        for (size_t i = 0; i < sh_contracts.size(); ++i)
            delete[] ppInsts[i];
        delete[] ppInsts;
    }

    if (!sz_contracts.empty())
    {
        char **ppInsts = new char *[sz_contracts.size()];
        for (size_t i = 0; i < sz_contracts.size(); ++i)
        {
            ppInsts[i] = new char[XTP_TICKER_LEN];
            strcpy(ppInsts[i], sz_contracts[i].c_str());
        }
        _md_api->SubscribeMarketData(ppInsts, sz_contracts.size(), XTP_EXCHANGE_SZ);
        // 同时订阅深交所逐笔（委托/成交）
        _md_api->SubscribeTickByTick(ppInsts, sz_contracts.size(), XTP_EXCHANGE_SZ);
        for (size_t i = 0; i < sz_contracts.size(); ++i)
            delete[] ppInsts[i];
        delete[] ppInsts;
    }
}

void xtp_market::unsubscribe()
{
    // Implementation similar to subscribe but using UnSubscribeMarketData
}

void xtp_market::OnDisconnected(int reason)
{
    std::cout << "xtp md disconnected, reason=" << reason << std::endl;
    _is_ready.store(false);
}

void xtp_market::OnError(XTPRI *error_info)
{
    if (error_info && error_info->error_id != 0)
    {
        std::cout << "xtp md error: " << error_info->error_msg << std::endl;
    }
}

void xtp_market::OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{
    if (error_info && error_info->error_id != 0)
    {
        std::cout << "xtp sub market data error: " << error_info->error_msg << std::endl;
    }
}

void xtp_market::OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last)
{}

void xtp_market::OnDepthMarketData(XTPMD *ptr, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count,
                                   int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count)
{
    auto it = _previous_tick_map.find(ptr->ticker);
    if (it == _previous_tick_map.end())
    {
        _previous_tick_map[ptr->ticker] = *ptr;
        return;
    }
    strcpy(_tick.instrument_id, ptr->ticker);
    // Convert data_time (int64 YYYYMMDDHHMMSSsss) to update_time and update_millisec
    long long t = ptr->data_time;
    _tick.update_millisec = t % 1000;
    t /= 1000;
    int ss = t % 100;
    t /= 100;
    int mm = t % 100;
    t /= 100;
    int hh = t % 100;
    sprintf(_tick.update_time, "%02d:%02d:%02d", hh, mm, ss);
    _tick.pre_close_price = ptr->pre_close_price;
    _tick.pre_settlement_price = ptr->pre_settl_price;
    _tick.last_price = ptr->last_price;
    _tick.volume = ptr->qty;
    _tick.last_volume = ptr->qty - it->second.qty;
    _tick.open_interest = ptr->total_long_positon;
    _tick.last_open_interest = ptr->total_long_positon - it->second.total_long_positon;
    _tick.open_price = ptr->open_price;
    _tick.highest_price = ptr->high_price;
    _tick.lowest_price = ptr->low_price;
    _tick.upper_limit_price = ptr->upper_limit_price;
    _tick.lower_limit_price = ptr->lower_limit_price;

    for (int i = 0; i < 10; ++i)
    {
        _tick.bid_price[i] = ptr->bid[i];
        _tick.bid_volume[i] = ptr->bid_qty[i];
        _tick.ask_price[i] = ptr->ask[i];
        _tick.ask_volume[i] = ptr->ask_qty[i];
    }

    if (ptr->last_price >= it->second.ask[0] || ptr->last_price >= ptr->ask[0])
    {
        _tick.tape_dir = eTapeDir::Up;
    } else if (ptr->last_price <= it->second.bid[0] || ptr->last_price <= ptr->bid[0])
    {
        _tick.tape_dir = eTapeDir::Down;
    } else
    {
        _tick.tape_dir = eTapeDir::Flat;
    }

    this->insert_event(_tick);
    it->second = *ptr;
}

void xtp_market::OnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last)
{
    // 逐笔订阅失败时输出错误信息
    if (error_info && error_info->error_id != 0)
    {
        std::cout << "xtp sub tick by tick error: " << error_info->error_msg << std::endl;
    }
}

void xtp_market::OnUnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last)
{}

void xtp_market::OnTickByTick(XTPTBT *tbt_data)
{
    // 防御性判断，避免空指针
    if (!tbt_data)
    {
        return;
    }

    // 将XTP时间戳(YYYYMMDDHHMMSSsss)拆分为秒级时间和毫秒
    long long t = tbt_data->data_time;
    int update_millisec = static_cast<int>(t % 1000);
    t /= 1000;
    int ss = static_cast<int>(t % 100);
    t /= 100;
    int mm = static_cast<int>(t % 100);
    t /= 100;
    int hh = static_cast<int>(t % 100);

    if (tbt_data->type == XTP_TBT_ENTRUST)
    {
        // 逐笔委托：转换为框架内部结构并派发
        TickByTickEntrustData entrust{};
        strcpy(entrust.instrument_id, tbt_data->ticker);
        sprintf(entrust.update_time, "%02d:%02d:%02d", hh, mm, ss);
        entrust.update_millisec = update_millisec;
        entrust.channel_no = tbt_data->entrust.channel_no;
        entrust.seq = tbt_data->entrust.seq;
        entrust.price = tbt_data->entrust.price;
        entrust.qty = tbt_data->entrust.qty;
        entrust.side = tbt_data->entrust.side;
        entrust.ord_type = tbt_data->entrust.ord_type;
        entrust.order_no = tbt_data->entrust.order_no;
        emit_tbt_entrust(entrust);
        return;
    }

    if (tbt_data->type == XTP_TBT_TRADE)
    {
        // 逐笔成交：转换为框架内部结构并派发
        TickByTickTradeData trade{};
        strcpy(trade.instrument_id, tbt_data->ticker);
        sprintf(trade.update_time, "%02d:%02d:%02d", hh, mm, ss);
        trade.update_millisec = update_millisec;
        trade.channel_no = tbt_data->trade.channel_no;
        trade.seq = tbt_data->trade.seq;
        trade.price = tbt_data->trade.price;
        trade.qty = tbt_data->trade.qty;
        trade.money = tbt_data->trade.money;
        trade.bid_no = tbt_data->trade.bid_no;
        trade.ask_no = tbt_data->trade.ask_no;
        trade.trade_flag = tbt_data->trade.trade_flag;
        emit_tbt_trade(trade);
    }
}
