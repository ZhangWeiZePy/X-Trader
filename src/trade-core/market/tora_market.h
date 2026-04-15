#pragma once

#include "market_api.h"
#include "TORATstpLev2MdApi.h"
#include "dyn_lib_loader.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>

class tora_market : public market_api, public TORALEV2API::CTORATstpLev2MdSpi
{
public:
    struct config_market_tora
    {
        std::string user_id;
        std::string password;
        std::string front_addr;
    };
    tora_market(std::map<std::string, std::string> &config, std::set<std::string> &contracts);
    virtual ~tora_market();
    virtual void release() override;

private:

    void subscribe();

public:
    virtual void OnFrontConnected() override;
    virtual void OnFrontDisconnected(int nReason) override;
    virtual void OnRspUserLogin(TORALEV2API::CTORATstpRspUserLoginField *pRspUserLogin, TORALEV2API::CTORATstpRspInfoField *pRspInfo, int nRequestID, bool bIsLast) override;
    virtual void OnRtnMarketData(TORALEV2API::CTORATstpLev2MarketDataField *pMarketData, const int FirstLevelBuyNum, const int FirstLevelBuyOrderVolumes[], const int FirstLevelSellNum, const int FirstLevelSellOrderVolumes[]) override;
    virtual void OnRtnTransaction(TORALEV2API::CTORATstpLev2TransactionField *pTransaction) override;
    virtual void OnRtnOrderDetail(TORALEV2API::CTORATstpLev2OrderDetailField *pOrderDetail) override;

private:
    TORALEV2API::CTORATstpLev2MdApi *_md_api;
    DynLibLoader _loader;
    config_market_tora _cfg{};

    std::set<std::string> _contracts;
    std::unordered_map<std::string, TORALEV2API::CTORATstpLev2MarketDataField> _previous_tick_map{};
};
