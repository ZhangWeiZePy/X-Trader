#pragma once

#include "market_api.h"
#include "xtp_quote_api.h"
#include "dyn_lib_loader.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>

class xtp_market : public market_api, public XTP::API::QuoteSpi
{
public:
    xtp_market(std::map<std::string, std::string> &config, std::set<std::string> &contracts);

    virtual ~xtp_market();

    virtual void release() override;

private:
    void subscribe();

    void unsubscribe();

public:
    ///当客户端与行情后台通信连接断开时，该方法被调用。
    virtual void OnDisconnected(int reason) override;

    ///错误应答
    virtual void OnError(XTPRI *error_info) override;

    ///订阅行情应答，包括股票、指数和期权
    virtual void OnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last) override;

    ///退订行情应答，包括股票、指数和期权
    virtual void OnUnSubMarketData(XTPST *ticker, XTPRI *error_info, bool is_last) override;

    ///深度行情通知，包含买一卖一队列
    virtual void OnDepthMarketData(XTPMD *market_data, int64_t bid1_qty[], int32_t bid1_count, int32_t max_bid1_count,
                                   int64_t ask1_qty[], int32_t ask1_count, int32_t max_ask1_count) override;

    ///订阅逐笔应答
    virtual void OnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last) override;

    ///退订逐笔应答
    virtual void OnUnSubTickByTick(XTPST *ticker, XTPRI *error_info, bool is_last) override;

    ///逐笔数据通知（委托/成交）
    virtual void OnTickByTick(XTPTBT *tbt_data) override;

private:
    XTP::API::QuoteApi *_md_api;
    DynLibLoader _loader;
    std::string _server_ip;
    int _server_port;
    std::string _user_id;
    std::string _password;
    int _client_id;
    XTP_PROTOCOL_TYPE _protocol_type;

    std::set<std::string> _contracts;
    std::unordered_map<std::string, XTPMD> _previous_tick_map{};
};
