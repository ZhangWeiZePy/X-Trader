#pragma once

#include "trader_api.h"
#include "xtp_trader_api.h"
#include "dyn_lib_loader.h"
#include <map>
#include <set>
#include <string>
#include <vector>

struct config_trader_xtp
{
    std::string server_ip;
    int server_port{0};
    std::string user_id;
    std::string password;
    int client_id{1};
    XTP_PROTOCOL_TYPE protocol_type{XTP_PROTOCOL_TCP};
};

class xtp_trader : public trader_api, public XTP::API::TraderSpi
{
public:
    /// @brief 构造函数，初始化并连接 XTP 交易接口
    /// @param config 包含登录信息和服务器地址的配置映射
    /// @param contracts 策略订阅的合约集合
    xtp_trader(std::map<std::string, std::string> &config, std::set<std::string> &contracts);

    /// @brief 析构函数，清理资源
    virtual ~xtp_trader();

    /// @brief 释放交易接口实例，断开连接并清理指针
    virtual void release() override;

public:
    /// @brief 获取当前交易日
    /// @return 格式为 "YYYYMMDD" 的交易日字符串
    virtual std::string get_trading_day() const override;

    /// @brief 请求查询资金账户信息
    virtual void get_account() override;

    /// @brief 获取当前交易器的缓存数据（合约、持仓、委托）
    /// @param i_map 返回的合约信息映射
    /// @param p_map 返回的持仓信息映射
    /// @param o_map 返回的委托信息映射
    virtual void get_trader_data(InstrumentMap &i_map, PositionMap &p_map, OrderMap &o_map) override;

    /// @brief 报单录入请求
    /// @param order_flag 报单标志（如限价单、市价单）
    /// @param contract 合约代码
    /// @param dir_offset 买卖开平方向
    /// @param price 报单价格
    /// @param volume 报单数量
    /// @return 返回框架内部维护的订单引用ID，若失败返回 null_orderref
    virtual orderref_t insert_order(eOrderFlag order_flag, const std::string &contract, eDirOffset dir_offset,
                                    double price, int volume) override;

    /// @brief 报单撤销请求
    /// @param order_ref 需要撤销的订单引用ID
    /// @return 撤单请求是否成功发送
    virtual bool cancel_order(const orderref_t order_ref) override;

public:
    /// @brief 当客户端与交易后台通信连接断开时触发的回调
    /// @param session_id 资金账户对应的 session_id
    /// @param reason 错误原因代码
    virtual void OnDisconnected(uint64_t session_id, int reason) override;

    /// @brief 错误应答回调
    /// @param error_info 包含错误代码和错误信息的结构体
    virtual void OnError(XTPRI *error_info) override;

    /// @brief 报单通知回调（订单状态发生变化时触发）
    /// @param order_info 订单的具体信息
    /// @param error_info 订单发生错误时的错误信息
    /// @param session_id 资金账户对应的 session_id
    virtual void OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id) override;

    /// @brief 成交通知回调（订单发生部分或全部成交时触发）
    /// @param trade_info 成交的具体信息
    /// @param session_id 资金账户对应的 session_id
    virtual void OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id) override;

    /// @brief 撤单出错通知回调
    /// @param cancel_info 撤单失败时的具体信息
    /// @param error_info 撤单发生错误时的错误信息
    /// @param session_id 资金账户对应的 session_id
    virtual void OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id) override;

    /// @brief 请求查询股票持仓响应回调
    /// @param position 查询到的持仓信息
    /// @param error_info 查询发生错误时的错误信息
    /// @param request_id 查询请求对应的 ID
    /// @param is_last 是否为该次查询的最后一条记录
    /// @param session_id 资金账户对应的 session_id
    virtual void OnQueryPosition(XTPQueryStkPositionRsp *position, XTPRI *error_info, int request_id, bool is_last,
                                 uint64_t session_id) override;

    /// @brief 请求查询资金账户响应回调
    /// @param asset 查询到的资金资产信息
    /// @param error_info 查询发生错误时的错误信息
    /// @param request_id 查询请求对应的 ID
    /// @param is_last 是否为该次查询的最后一条记录
    /// @param session_id 资金账户对应的 session_id
    virtual void OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id) override;

    virtual void OnQueryOrder(XTPQueryOrderRsp *order_info, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id) override;

    /// @brief 请求查询报单响应回调（包含详细信息）
    /// @param order_info 查询到的报单详细信息
    /// @param error_info 查询发生错误时的错误信息
    /// @param request_id 查询请求对应的 ID
    /// @param is_last 是否为该次查询的最后一条记录
    /// @param session_id 资金账户对应的 session_id
    virtual void OnQueryOrderEx(XTPOrderInfoEx *order_info, XTPRI *error_info, int request_id, bool is_last,
                                uint64_t session_id) override;

private:


    /// @brief 内部调用，主动请求查询持仓
    void req_qry_position();

    /// @brief 内部调用，主动请求查询报单
    void req_qry_order();

private:
    XTP::API::TraderApi *_td_api;
    DynLibLoader _loader;
    config_trader_xtp _cfg{};
    uint64_t _session_id;
    std::string _trading_day;

    std::set<std::string> _contracts;
    InstrumentMap _instrument_map;
    PositionMap _position_map;
    OrderMap _order_map;
    std::atomic<uint64_t> _order_ref;
};
