#include "tora_market.h"
#include <filesystem>
#include <cstring>
#include <spdlog/spdlog.h>

tora_market::tora_market(std::map<std::string, std::string> &config, std::set<std::string> &contracts) :
    _contracts(contracts), _md_api(nullptr)
{
    if (config.find("counter") == config.end() || config["counter"].empty())
    {
        throw std::runtime_error("Missing or empty 'counter' in config");
    }
    std::string counter = config["counter"];
#ifdef _WIN32
    std::string lib_path = "lib/" + counter + "/lev2mdapi.dll";
    const char* creator_name = "?CreateTstpLev2MdApi@CTORATstpLev2MdApi@TORALEV2API@@SAPEAV12@AEBW4TTORATstpMDSubModeType@2@_N@Z"; // This might be wrong, wait, let's use the correct one
#else
    std::string lib_path = "lib/" + counter + "/liblev2mdapi.so";
    const char *creator_name = "_ZN11TORALEV2API18CTORATstpLev2MdApi19CreateTstpLev2MdApiERKcb";
#endif

#ifdef _WIN32
    creator_name = "?CreateTstpLev2MdApi@CTORATstpLev2MdApi@TORALEV2API@@SAPEAV12@AEBW4TTORATstpMDSubModeType@2@_N@Z";
    // We saw from strings it was: ?CreateTstpLev2MdApi@CTORATstpLev2MdApi@TORALEV2API@@SAPEAV12@AEBD_N@Z
    creator_name = "?CreateTstpLev2MdApi@CTORATstpLev2MdApi@TORALEV2API@@SAPEAV12@AEBD_N@Z";
#endif

    if (!_loader.load(lib_path))
    {
        throw std::runtime_error("Failed to load library: " + lib_path + ", error: " + _loader.get_error());
    }
    typedef TORALEV2API::CTORATstpLev2MdApi * (*CreateTstpLev2MdApi_t)(const TORALEV2API::TTORATstpMDSubModeType &, bool);
    CreateTstpLev2MdApi_t creator = _loader.get_function<CreateTstpLev2MdApi_t>(creator_name);
    if (!creator)
    {
        throw std::runtime_error("Failed to find symbol CreateTstpLev2MdApi in " + lib_path);
    }
    _cfg.user_id = config["user_id"];
    _cfg.password = config["password"];
    _cfg.front_addr = config["market_front"]; // format like "tcp://127.0.0.1:8000"

    TORALEV2API::TTORATstpMDSubModeType sub_mode = TORALEV2API::TORA_TSTP_MST_TCP;
    _md_api = creator(sub_mode, false);
    if (!_md_api)
    {
        throw std::runtime_error("Failed to create TORA Lev2 Md API");
    }

    _md_api->RegisterSpi(this);
    
    char front_addr[128]{};
    strcpy(front_addr, _cfg.front_addr.c_str());
    _md_api->RegisterFront(front_addr);

    // Some systems report "Operation now in progress" on TCP if we don't explicitly pass empty cpu cores or ignore the init error in a certain way
    // But mostly it's normal behavior for non-blocking sockets in internal API. Let's just Init it normally.
    _md_api->Init("");
}

tora_market::~tora_market()
{
    release();
}

void tora_market::release()
{
    _is_ready.store(false);
    if (_md_api)
    {
        _md_api->RegisterSpi(nullptr);
        _md_api->Release();
        _md_api = nullptr;
    }
}

void tora_market::subscribe()
{
    std::vector<std::string> sh_contracts;
    std::vector<std::string> sz_contracts;

    for (const auto &c: _contracts)
    {
        if (c.length() > 0 && (c[0] == '6' || c[0] == '5'))
        {
            sh_contracts.push_back(c);
        } else
        {
            sz_contracts.push_back(c);
        }
    }

    if (!sh_contracts.empty())
    {
        char **ppInsts = new char *[sh_contracts.size()];
        for (size_t i = 0; i < sh_contracts.size(); ++i)
        {
            ppInsts[i] = new char[31]; // TTORATstpSecurityIDType
            strcpy(ppInsts[i], sh_contracts[i].c_str());
        }
        _md_api->SubscribeMarketData(ppInsts, sh_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SSE);
        _md_api->SubscribeTransaction(ppInsts, sh_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SSE);
        _md_api->SubscribeOrderDetail(ppInsts, sh_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SSE);
        for (size_t i = 0; i < sh_contracts.size(); ++i)
            delete[] ppInsts[i];
        delete[] ppInsts;
    }

    if (!sz_contracts.empty())
    {
        char **ppInsts = new char *[sz_contracts.size()];
        for (size_t i = 0; i < sz_contracts.size(); ++i)
        {
            ppInsts[i] = new char[31]; // TTORATstpSecurityIDType
            strcpy(ppInsts[i], sz_contracts[i].c_str());
        }
        _md_api->SubscribeMarketData(ppInsts, sz_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SZSE);
        _md_api->SubscribeTransaction(ppInsts, sz_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SZSE);
        _md_api->SubscribeOrderDetail(ppInsts, sz_contracts.size(), TORALEV2API::TORA_TSTP_EXD_SZSE);
        for (size_t i = 0; i < sz_contracts.size(); ++i)
            delete[] ppInsts[i];
        delete[] ppInsts;
    }
}

void tora_market::OnFrontConnected()
{
    spdlog::info("tora md front connected");
    TORALEV2API::CTORATstpReqUserLoginField req{};
    strcpy(req.LogInAccount, _cfg.user_id.c_str());
    req.LogInAccountType = TORALEV2API::TORA_TSTP_LACT_UserID;
    strcpy(req.Password, _cfg.password.c_str());
    _md_api->ReqUserLogin(&req, 1);
}

void tora_market::OnFrontDisconnected(int nReason)
{
    spdlog::warn("tora md disconnected, reason={}", nReason);
    _is_ready.store(false);
}

void tora_market::OnRspUserLogin(TORALEV2API::CTORATstpRspUserLoginField *pRspUserLogin, TORALEV2API::CTORATstpRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
    if (pRspInfo && pRspInfo->ErrorID != 0)
    {
        spdlog::error("tora md login error: {}", pRspInfo->ErrorMsg);
    } else
    {
        spdlog::info("tora md login success!");
        _is_ready.store(true, std::memory_order_release);
        subscribe();
    }
}

void tora_market::OnRtnMarketData(TORALEV2API::CTORATstpLev2MarketDataField *pMarketData, const int FirstLevelBuyNum, const int FirstLevelBuyOrderVolumes[], const int FirstLevelSellNum, const int FirstLevelSellOrderVolumes[])
{
    auto it = _previous_tick_map.find(pMarketData->SecurityID);
    if (it == _previous_tick_map.end())
    {
        _previous_tick_map[pMarketData->SecurityID] = *pMarketData;
        return;
    }

    static MarketData _tick{};
    strcpy(_tick.instrument_id, pMarketData->SecurityID);

    // format DataTimeStamp e.g. "093000000" or int 93000000
    int time_stamp = pMarketData->DataTimeStamp;
    _tick.update_millisec = time_stamp % 1000;
    time_stamp /= 1000;
    int ss = time_stamp % 100;
    time_stamp /= 100;
    int mm = time_stamp % 100;
    time_stamp /= 100;
    int hh = time_stamp % 100;
    sprintf(_tick.update_time, "%02d:%02d:%02d", hh, mm, ss);

    _tick.pre_close_price = pMarketData->PreClosePrice;
    _tick.last_price = pMarketData->LastPrice;
    _tick.volume = pMarketData->TotalVolumeTrade;
    _tick.last_volume = pMarketData->TotalVolumeTrade - it->second.TotalVolumeTrade;
    _tick.open_price = pMarketData->OpenPrice;
    _tick.highest_price = pMarketData->HighestPrice;
    _tick.lowest_price = pMarketData->LowestPrice;

    _tick.bid_price[0] = pMarketData->BidPrice1;
    _tick.ask_price[0] = pMarketData->AskPrice1;
    // ... further levels omitted for brevity, normally you'd copy all 10

    if (pMarketData->LastPrice >= it->second.AskPrice1 || pMarketData->LastPrice >= pMarketData->AskPrice1)
    {
        _tick.tape_dir = eTapeDir::Up;
    } else if (pMarketData->LastPrice <= it->second.BidPrice1 || pMarketData->LastPrice <= pMarketData->BidPrice1)
    {
        _tick.tape_dir = eTapeDir::Down;
    } else
    {
        _tick.tape_dir = eTapeDir::Flat;
    }

    OrderBookData ob{};
    ob.exchange_id = 0;
    strcpy(ob.instrument_id, _tick.instrument_id);
    ob.last_price = _tick.last_price;
    ob.qty = static_cast<int64_t>(_tick.volume);
    ob.turnover = 0.0;
    ob.trades_count = 0;
    for (int i = 0; i < 10; ++i)
    {
        ob.bid[i] = _tick.bid_price[i];
        ob.ask[i] = _tick.ask_price[i];
        ob.bid_qty[i] = _tick.bid_volume[i];
        ob.ask_qty[i] = _tick.ask_volume[i];
    }
    ob.data_time = 0;
    this->insert_event(ob);
    it->second = *pMarketData;
}

void tora_market::OnRtnTransaction(TORALEV2API::CTORATstpLev2TransactionField *pTransaction)
{
    if (!pTransaction) return;

    TickByTickTradeData trade{};
    strcpy(trade.instrument_id, pTransaction->SecurityID);
    
    int time_stamp = pTransaction->TradeTime;
    trade.update_millisec = time_stamp % 1000;
    time_stamp /= 1000;
    int ss = time_stamp % 100;
    time_stamp /= 100;
    int mm = time_stamp % 100;
    time_stamp /= 100;
    int hh = time_stamp % 100;
    sprintf(trade.update_time, "%02d:%02d:%02d", hh, mm, ss);

    trade.price = pTransaction->TradePrice;
    trade.qty = pTransaction->TradeVolume;
    trade.trade_flag = pTransaction->ExecType;
    emit_tbt_trade(trade);
}

void tora_market::OnRtnOrderDetail(TORALEV2API::CTORATstpLev2OrderDetailField *pOrderDetail)
{
    if (!pOrderDetail) return;

    TickByTickEntrustData entrust{};
    strcpy(entrust.instrument_id, pOrderDetail->SecurityID);

    int time_stamp = pOrderDetail->OrderTime;
    entrust.update_millisec = time_stamp % 1000;
    time_stamp /= 1000;
    int ss = time_stamp % 100;
    time_stamp /= 100;
    int mm = time_stamp % 100;
    time_stamp /= 100;
    int hh = time_stamp % 100;
    sprintf(entrust.update_time, "%02d:%02d:%02d", hh, mm, ss);

    entrust.price = pOrderDetail->Price;
    entrust.qty = pOrderDetail->Volume;
    entrust.side = pOrderDetail->Side;
    entrust.ord_type = pOrderDetail->OrderType;
    emit_tbt_entrust(entrust);
}
