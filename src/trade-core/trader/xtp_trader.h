#pragma once

#include "trader_api.h"
#include "xtp_trader_api.h"
#include "dyn_lib_loader.h"
#include <map>
#include <set>
#include <string>
#include <vector>

class xtp_trader : public trader_api, public XTP::API::TraderSpi
{
public:
    xtp_trader(std::map<std::string, std::string> &config, std::set<std::string> &contracts);

    virtual ~xtp_trader();

    virtual void release() override;

public:
    virtual std::string get_trading_day() const override;

    virtual void get_account() override;

    virtual void get_trader_data(InstrumentMap &i_map, PositionMap &p_map, OrderMap &o_map) override;

    virtual orderref_t insert_order(eOrderFlag order_flag, const std::string &contract, eDirOffset dir_offset,
                                    double price, int volume) override;

    virtual bool cancel_order(const orderref_t order_ref) override;

public:
    virtual void OnDisconnected(uint64_t session_id, int reason) override;

    virtual void OnError(XTPRI *error_info) override;

    virtual void OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id) override;

    virtual void OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id) override;

    virtual void OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id) override;

    virtual void OnQueryPosition(XTPQueryStkPositionRsp *position, XTPRI *error_info, int request_id, bool is_last,
                                 uint64_t session_id) override;

    virtual void OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id) override;

    virtual void OnQueryOrderEx(XTPOrderInfoEx *order_info, XTPRI *error_info, int request_id, bool is_last,
                                uint64_t session_id) override;

private:
    void req_qry_position();

    void req_qry_order();

private:
    XTP::API::TraderApi *_td_api;
    DynLibLoader _loader;
    std::string _server_ip;
    int _server_port;
    std::string _user_id;
    std::string _password;
    int _client_id;
    uint64_t _session_id;
    std::string _trading_day;
    XTP_PROTOCOL_TYPE _protocol_type;

    std::set<std::string> _contracts;
    InstrumentMap _instrument_map;
    PositionMap _position_map;
    OrderMap _order_map;
    std::atomic<uint64_t> _order_ref;
};
