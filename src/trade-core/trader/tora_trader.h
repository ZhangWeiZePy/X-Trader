#pragma once

#include "trader_api.h"
#include "TORATstpTraderApi.h"
#include "dyn_lib_loader.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class tora_trader : public trader_api, public TORASTOCKAPI::CTORATstpTraderSpi
{
public:
    tora_trader(std::map<std::string, std::string> &config, std::set<std::string> &contracts);

    virtual ~tora_trader();

    virtual void release() override;

public:
    virtual std::string get_trading_day() const override;

    virtual void get_account() override;

    virtual void get_trader_data(InstrumentMap &i_map, PositionMap &p_map, OrderMap &o_map) override;

    virtual orderref_t insert_order(eOrderFlag order_flag, const std::string &contract, eDirOffset dir_offset,
                                    double price, int volume) override;

    virtual bool cancel_order(const orderref_t order_ref) override;

public:
    virtual void OnFrontConnected() override;
    virtual void OnFrontDisconnected(int nReason) override;
    virtual void OnRspUserLogin(TORASTOCKAPI::CTORATstpRspUserLoginField *pRspUserLoginField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) override;
    
    virtual void OnRspOrderInsert(TORASTOCKAPI::CTORATstpInputOrderField *pInputOrderField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) override;
    virtual void OnRtnOrder(TORASTOCKAPI::CTORATstpOrderField *pOrderField) override;
    virtual void OnErrRtnOrderInsert(TORASTOCKAPI::CTORATstpInputOrderField *pInputOrderField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) override;
    
    virtual void OnRtnTrade(TORASTOCKAPI::CTORATstpTradeField *pTradeField) override;
    virtual void OnRspOrderAction(TORASTOCKAPI::CTORATstpInputOrderActionField *pInputOrderActionField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) override;
    virtual void OnErrRtnOrderAction(TORASTOCKAPI::CTORATstpInputOrderActionField *pInputOrderActionField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) override;

    virtual void OnRspQryPosition(TORASTOCKAPI::CTORATstpPositionField *pPositionField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID, bool bIsLast) override;
    virtual void OnRspQryOrder(TORASTOCKAPI::CTORATstpOrderField *pOrderField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID, bool bIsLast) override;
    virtual void OnRspQryTradingAccount(TORASTOCKAPI::CTORATstpTradingAccountField *pTradingAccountField, TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID, bool bIsLast) override;

private:
    void req_qry_position();
    void req_qry_order();

private:
    TORASTOCKAPI::CTORATstpTraderApi *_td_api;
    DynLibLoader _loader;
    std::string _user_id;
    std::string _password;
    std::string _front_addr;
    std::string _department_id;
    std::string _shareholder_id_sh;
    std::string _shareholder_id_sz;

    std::set<std::string> _contracts;
    std::atomic<uint32_t> _order_ref;
    int _session_id;

    InstrumentMap _instrument_map;
    PositionMap _position_map;
    OrderMap _order_map;
};