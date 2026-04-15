#include "xtp_trader.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

namespace
{
    std::atomic<int> g_xtp_request_id{1};

    int next_request_id()
    {
        return g_xtp_request_id.fetch_add(1, std::memory_order_relaxed);
    }

    template<size_t N>
    void copy_text(char (&dst)[N], const char *src)
    {
        if (src == nullptr)
        {
            dst[0] = '\0';
            return;
        }

        std::snprintf(dst, N, "%s", src);
    }

    template<size_t N>
    void copy_text(char (&dst)[N], const std::string &src)
    {
        std::snprintf(dst, N, "%s", src.c_str());
    }

    XTP_MARKET_TYPE detect_market(const std::string &contract)
    {
        if (!contract.empty() && (contract[0] == '5' || contract[0] == '6'))
        {
            return XTP_MKT_SH_A;
        }
        return XTP_MKT_SZ_A;
    }

    void fill_exchange_id(XTP_MARKET_TYPE market, ExchangeIDType &exchange_id)
    {
        if (market == XTP_MKT_SH_A)
        {
            copy_text(exchange_id, "SSE");
        } else if (market == XTP_MKT_SZ_A)
        {
            copy_text(exchange_id, "SZSE");
        } else
        {
            exchange_id[0] = '\0';
        }
    }

    void fill_time(int64_t xtp_time, TimeType &time_text)
    {
        if (xtp_time <= 0)
        {
            time_text[0] = '\0';
            return;
        }

        std::string text = std::to_string(xtp_time);
        if (text.size() < 14)
        {
            time_text[0] = '\0';
            return;
        }

        const size_t hour_pos = text.size() - 9;
        std::snprintf(time_text, sizeof(TimeType), "%c%c:%c%c:%c%c",
                      text[hour_pos], text[hour_pos + 1],
                      text[hour_pos + 2], text[hour_pos + 3],
                      text[hour_pos + 4], text[hour_pos + 5]);
    }

    void to_xtp_direction(eDirOffset dir_offset, XTPOrderInsertInfo &req)
    {
        switch (dir_offset)
        {
            case eDirOffset::BuyOpen:
                req.side = XTP_SIDE_BUY;
                req.position_effect = XTP_POSITION_EFFECT_OPEN;
                break;
            case eDirOffset::SellOpen:
                req.side = XTP_SIDE_SELL;
                req.position_effect = XTP_POSITION_EFFECT_OPEN;
                break;
            case eDirOffset::BuyCloseToday:
                req.side = XTP_SIDE_BUY;
                req.position_effect = XTP_POSITION_EFFECT_CLOSETODAY;
                break;
            case eDirOffset::SellCloseToday:
                req.side = XTP_SIDE_SELL;
                req.position_effect = XTP_POSITION_EFFECT_CLOSETODAY;
                break;
            case eDirOffset::BuyCloseYesterday:
                req.side = XTP_SIDE_BUY;
                req.position_effect = XTP_POSITION_EFFECT_CLOSEYESTERDAY;
                break;
            case eDirOffset::SellCloseYesterday:
                req.side = XTP_SIDE_SELL;
                req.position_effect = XTP_POSITION_EFFECT_CLOSEYESTERDAY;
                break;
            case eDirOffset::BuyClose:
                req.side = XTP_SIDE_BUY;
                req.position_effect = XTP_POSITION_EFFECT_CLOSE;
                break;
            case eDirOffset::SellClose:
                req.side = XTP_SIDE_SELL;
                req.position_effect = XTP_POSITION_EFFECT_CLOSE;
                break;
        }
    }

    eDirOffset to_local_direction(XTP_SIDE_TYPE side, XTP_POSITION_EFFECT_TYPE position_effect)
    {
        if (side == XTP_SIDE_BUY)
        {
            switch (position_effect)
            {
                case XTP_POSITION_EFFECT_OPEN:
                    return eDirOffset::BuyOpen;
                case XTP_POSITION_EFFECT_CLOSETODAY:
                    return eDirOffset::BuyCloseToday;
                case XTP_POSITION_EFFECT_CLOSEYESTERDAY:
                    return eDirOffset::BuyCloseYesterday;
                case XTP_POSITION_EFFECT_CLOSE:
                case XTP_POSITION_EFFECT_INIT:
                default:
                    return eDirOffset::BuyClose;
            }
        }

        switch (position_effect)
        {
            case XTP_POSITION_EFFECT_OPEN:
                return eDirOffset::SellOpen;
            case XTP_POSITION_EFFECT_CLOSETODAY:
                return eDirOffset::SellCloseToday;
            case XTP_POSITION_EFFECT_CLOSEYESTERDAY:
                return eDirOffset::SellCloseYesterday;
            case XTP_POSITION_EFFECT_CLOSE:
            case XTP_POSITION_EFFECT_INIT:
            default:
                return eDirOffset::SellClose;
        }
    }

    XTP_PRICE_TYPE to_xtp_price_type(eOrderFlag order_flag, XTP_MARKET_TYPE market)
    {
        switch (order_flag)
        {
            case eOrderFlag::Limit:
                return XTP_PRICE_LIMIT;
            case eOrderFlag::Market:
                return market == XTP_MKT_SH_A ? XTP_PRICE_BEST5_OR_LIMIT : XTP_PRICE_BEST_OR_CANCEL;
            case eOrderFlag::FOK:
                return XTP_PRICE_ALL_OR_CANCEL;
            case eOrderFlag::FAK:
                return market == XTP_MKT_SH_A ? XTP_PRICE_BEST5_OR_CANCEL : XTP_PRICE_BEST5_OR_CANCEL;
            default:
                return XTP_PRICE_LIMIT;
        }
    }

    eOrderFlag to_local_order_flag(XTP_PRICE_TYPE price_type)
    {
        switch (price_type)
        {
            case XTP_PRICE_ALL_OR_CANCEL:
                return eOrderFlag::FOK;
            case XTP_PRICE_BEST5_OR_CANCEL:
            case XTP_PRICE_BEST_OR_CANCEL:
                return eOrderFlag::FAK;
            case XTP_PRICE_LIMIT:
            default:
                return eOrderFlag::Limit;
        }
    }

    eOrderStatus to_local_order_status(XTP_ORDER_STATUS_TYPE status)
    {
        switch (status)
        {
            case XTP_ORDER_STATUS_ALLTRADED:
                return eOrderStatus::AllTraded;
            case XTP_ORDER_STATUS_PARTTRADEDQUEUEING:
                return eOrderStatus::PartTradedQueueing;
            case XTP_ORDER_STATUS_PARTTRADEDNOTQUEUEING:
                return eOrderStatus::PartTradedNotQueueing;
            case XTP_ORDER_STATUS_NOTRADEQUEUEING:
                return eOrderStatus::NoTradeQueueing;
            case XTP_ORDER_STATUS_CANCELED:
            case XTP_ORDER_STATUS_REJECTED:
                return eOrderStatus::Canceled;
            case XTP_ORDER_STATUS_INIT:
            case XTP_ORDER_STATUS_UNKNOWN:
            default:
                return eOrderStatus::Unknown;
        }
    }

    eOrderSubmitStatus to_local_submit_status(XTP_ORDER_SUBMIT_STATUS_TYPE status)
    {
        switch (status)
        {
            case XTP_ORDER_SUBMIT_STATUS_INSERT_SUBMITTED:
                return eOrderSubmitStatus::InsertSubmitted;
            case XTP_ORDER_SUBMIT_STATUS_INSERT_ACCEPTED:
                return eOrderSubmitStatus::Accepted;
            case XTP_ORDER_SUBMIT_STATUS_INSERT_REJECTED:
                return eOrderSubmitStatus::InsertRejected;
            case XTP_ORDER_SUBMIT_STATUS_CANCEL_SUBMITTED:
                return eOrderSubmitStatus::CancelSubmitted;
            case XTP_ORDER_SUBMIT_STATUS_CANCEL_REJECTED:
                return eOrderSubmitStatus::CancelRejected;
            case XTP_ORDER_SUBMIT_STATUS_CANCEL_ACCEPTED:
                return eOrderSubmitStatus::Accepted;
            default:
                return eOrderSubmitStatus::InsertSubmitted;
        }
    }

    bool has_error(const XTPRI *error_info)
    {
        return error_info != nullptr && error_info->error_id != 0;
    }

    void bump_order_ref(std::atomic<uint64_t> &order_ref_counter, orderref_t order_ref)
    {
        uint64_t expected = order_ref_counter.load(std::memory_order_relaxed);
        const uint64_t desired = order_ref + 1;
        while (expected < desired &&
               !order_ref_counter.compare_exchange_weak(expected, desired, std::memory_order_relaxed))
        {}
    }

    template<typename TOrderInfo>
    Order make_order_from_xtp(const TOrderInfo &order_info)
    {
        Order order{};
        order.order_ref = order_info.order_client_id;
        order.session_id = 0;
        fill_exchange_id(order_info.market, order.exchange_id);
        copy_text(order.instrument_id, order_info.ticker);
        order.dir_offset = to_local_direction(order_info.side, order_info.position_effect);
        order.order_flag = to_local_order_flag(order_info.price_type);
        order.limit_price = order_info.price;
        order.volume_total_original = static_cast<int>(order_info.quantity);
        order.volume_traded = static_cast<int>(order_info.qty_traded);
        order.volume_total = static_cast<int>(order_info.qty_left);
        copy_text(order.order_local_id, order_info.order_local_id);
        std::snprintf(order.order_sys_id, sizeof(order.order_sys_id), "%llu",
                      static_cast<unsigned long long>(order_info.order_xtp_id));
        order.order_submit_status = to_local_submit_status(order_info.order_submit_status);
        order.order_status = to_local_order_status(order_info.order_status);
        fill_time(order_info.insert_time, order.insert_time);
        fill_time(order_info.cancel_time, order.cancel_time);
        return order;
    }

    Order make_trade_order(const XTPTradeReport &trade_info)
    {
        Order order{};
        order.order_ref = trade_info.order_client_id;
        fill_exchange_id(trade_info.market, order.exchange_id);
        copy_text(order.instrument_id, trade_info.ticker);
        order.dir_offset = to_local_direction(trade_info.side, trade_info.position_effect);
        order.limit_price = trade_info.price;
        order.volume_total_original = static_cast<int>(trade_info.quantity);
        order.volume_traded = 0;
        order.volume_total = static_cast<int>(trade_info.quantity);
        std::snprintf(order.order_sys_id, sizeof(order.order_sys_id), "%llu",
                      static_cast<unsigned long long>(trade_info.order_xtp_id));
        return order;
    }

    orderref_t find_order_ref_by_xtp_id(const OrderMap &order_map, uint64_t order_xtp_id)
    {
        for (const auto &[order_ref, order]: order_map)
        {
            if (order.order_sys_id[0] == '\0')
            {
                continue;
            }

            if (std::strtoull(order.order_sys_id, nullptr, 10) == order_xtp_id)
            {
                return order_ref;
            }
        }

        return null_orderref;
    }
}

xtp_trader::xtp_trader(std::map<std::string, std::string> &config, std::set<std::string> &contracts) :
    _contracts(contracts), _td_api(nullptr), _order_ref(1)
{
    if (config.find("counter") == config.end() || config["counter"].empty())
    {
        throw std::runtime_error("Missing or empty 'counter' in config");
    }
    std::string counter = config["counter"];
#ifdef _WIN32
	std::string lib_path = "lib/" + counter + "/xtptraderapi.dll";
	const char* creator_name = "?CreateTraderApi@TraderApi@API@XTP@@SAPEAV123@EPEBDW4XTP_LOG_LEVEL@@@Z";
#else
    std::string lib_path = "lib/" + counter + "/libxtptraderapi.so";
    const char *creator_name = "_ZN3XTP3API9TraderApi15CreateTraderApiEhPKc13XTP_LOG_LEVEL";
#endif
    if (!_loader.load(lib_path))
    {
        throw std::runtime_error("Failed to load library: " + lib_path + ", error: " + _loader.get_error());
    }
    typedef XTP::API::TraderApi * (*CreateTraderApi_t)(uint8_t, const char *, XTP_LOG_LEVEL);
    CreateTraderApi_t creator = _loader.get_function<CreateTraderApi_t>(creator_name);
    if (!creator)
    {
        throw std::runtime_error("Failed to find symbol CreateTraderApi in " + lib_path);
    }
    _cfg.user_id = config["user_id"];
    _cfg.password = config["password"];
    _cfg.client_id = config.count("client_id") ? std::stoi(config["client_id"]) : 1;
    spdlog::info("xtp trader client_id: {}", _cfg.client_id);
    if (_cfg.client_id < 1 || _cfg.client_id > 99)
    {
        throw std::runtime_error("Invalid XTP client_id (must be 1~99): " + std::to_string(_cfg.client_id));
    }
    std::string front = config["trade_front"];
    size_t pos = front.find("://");
    if (pos != std::string::npos)
    {
        front = front.substr(pos + 3);
    }
    pos = front.find(":");
    if (pos != std::string::npos)
    {
        _cfg.server_ip = front.substr(0, pos);
        _cfg.server_port = std::stoi(front.substr(pos + 1));
    } else
    {
        _cfg.server_ip = front;
        _cfg.server_port = 0;
    }

    int sock_type = config.count("sock_type") ? std::stoi(config["sock_type"]) : 1;
    _cfg.protocol_type = (sock_type == 2) ? XTP_PROTOCOL_UDP : XTP_PROTOCOL_TCP;

    std::filesystem::path flow_dir = std::filesystem::path("flow") / "xtp" / "td" / _cfg.user_id;
    std::filesystem::create_directories(flow_dir);

    std::string flow_path = std::filesystem::absolute(flow_dir).string();
#ifdef _WIN32
    if (!flow_path.empty() && flow_path.back() != '\\' && flow_path.back() != '/')
    {
        flow_path.push_back('\\');
    }
#else
    if (!flow_path.empty() && flow_path.back() != '/')
    {
        flow_path.push_back('/');
    }
#endif

    const uint8_t client_id_u8 = static_cast<uint8_t>(_cfg.client_id);
    _td_api = creator(client_id_u8, flow_path.c_str(), XTP_LOG_LEVEL_INFO);
    if (!_td_api)
    {
        throw std::runtime_error("Failed to create XTP Trader API, lib_path=" + lib_path +
                                 ", client_id=" + std::to_string(_cfg.client_id) + ", flow_path=" + flow_path);
    }

    _td_api->RegisterSpi(this);
    _td_api->SetHeartBeatInterval(15);
    _td_api->SetSoftwareVersion("1.0.0");
    std::string software_key;
    if (config.count("software_key"))
    {
        software_key = config["software_key"];
    }
    if (software_key.empty() && config.count("auth_code"))
    {
        software_key = config["auth_code"];
    }
    if (!software_key.empty())
    {
        _td_api->SetSoftwareKey(software_key.c_str());
    }

    _session_id = _td_api->Login(_cfg.server_ip.c_str(), _cfg.server_port, _cfg.user_id.c_str(),
                                 _cfg.password.c_str(), _cfg.protocol_type);
    if (_session_id == 0)
    {
        XTPRI *error_info = _td_api->GetApiLastError();
        spdlog::error("xtp td login error: {}", (error_info ? error_info->error_msg : "unknown"));
    } else
    {
        spdlog::info("xtp td login success! session_id={}", _session_id);
        _trading_day = _td_api->GetTradingDay();
        _is_ready.store(true, std::memory_order_release);
        req_qry_position();
        req_qry_order();
    }
}

xtp_trader::~xtp_trader()
{
    release();
}

void xtp_trader::release()
{
    _is_ready.store(false);
    if (_td_api)
    {
        if (_session_id > 0)
        {
            _td_api->Logout(_session_id);
            _session_id = 0;
        }
        _td_api->RegisterSpi(nullptr);
        _td_api->Release();
        _td_api = nullptr;
    }
}

std::string xtp_trader::get_trading_day() const
{
    return _trading_day;
}

void xtp_trader::get_account()
{
    if (_td_api && _session_id > 0)
    {
        const int ret = _td_api->QueryAsset(_session_id, next_request_id());
        if (ret != 0)
        {
            auto *error_info = _td_api->GetApiLastError();
            spdlog::error("xtp account query error: {}", (error_info ? error_info->error_msg : "unknown"));
        }
    }
}

void xtp_trader::get_trader_data(InstrumentMap &i_map, PositionMap &p_map, OrderMap &o_map)
{
    i_map = _instrument_map;
    p_map = _position_map;
    o_map = _order_map;
}

orderref_t xtp_trader::insert_order(eOrderFlag order_flag, const std::string &contract, eDirOffset dir_offset,
                                    double price, int volume)
{
    if (!_td_api || _session_id == 0 || !_is_ready.load(std::memory_order_acquire))
    {
        return null_orderref;
    }
    XTPOrderInsertInfo req{};
    const orderref_t order_ref = _order_ref.fetch_add(1, std::memory_order_relaxed);
    req.order_client_id = static_cast<uint32_t>(order_ref);
    copy_text(req.ticker, contract);
    req.market = detect_market(contract);
    req.price = price;
    req.quantity = volume;
    req.price_type = to_xtp_price_type(order_flag, req.market);
    req.business_type = XTP_BUSINESS_TYPE_CASH;
    to_xtp_direction(dir_offset, req);
    const uint64_t xtp_id = _td_api->InsertOrder(&req, _session_id);
    if (xtp_id == 0)
    {
        auto *error_info = _td_api->GetApiLastError();
        spdlog::error("xtp td insert order error: {}", (error_info ? error_info->error_msg : "unknown"));
        return null_orderref;
    }
    Order o{};
    o.order_ref = order_ref;
    fill_exchange_id(req.market, o.exchange_id);
    copy_text(o.instrument_id, contract);
    o.dir_offset = dir_offset;
    o.order_flag = order_flag;
    o.limit_price = price;
    o.volume_total_original = volume;
    o.volume_total = volume;
    o.volume_traded = 0;
    o.order_submit_status = eOrderSubmitStatus::InsertSubmitted;
    o.order_status = eOrderStatus::Unknown;
    std::snprintf(o.order_sys_id, sizeof(o.order_sys_id), "%llu", static_cast<unsigned long long>(xtp_id));
    _order_map[order_ref] = o;
    return order_ref;
}

bool xtp_trader::cancel_order(const orderref_t order_ref)
{
    if (!_td_api || _session_id == 0 || !_is_ready.load(std::memory_order_acquire))
    {
        return false;
    }
    const auto it = _order_map.find(order_ref);
    if (it == _order_map.end())
    {
        spdlog::warn("xtp cancel order failed: cannot find order_ref {}", order_ref);
        return false;
    }
    auto &order = it->second;
    if (order.order_sys_id[0] == '\0')
    {
        spdlog::warn("xtp cancel order failed: order_sys_id is empty.");
        return false;
    }
    const uint64_t order_xtp_id = std::strtoull(order.order_sys_id, nullptr, 10);
    if (order_xtp_id == 0)
    {
        spdlog::warn("xtp cancel order failed: invalid order_xtp_id.");
        return false;
    }
    const uint64_t cancel_id = _td_api->CancelOrder(order_xtp_id, _session_id);
    if (cancel_id == 0)
    {
        auto *error_info = _td_api->GetApiLastError();
        spdlog::error("xtp td cancel order error: {}", (error_info ? error_info->error_msg : "unknown"));
        return false;
    }
    return true;
}

void xtp_trader::req_qry_position()
{
    if (_td_api && _session_id > 0)
    {
        const int ret = _td_api->QueryPosition("", _session_id, next_request_id());
        if (ret != 0)
        {
            auto *error_info = _td_api->GetApiLastError();
            spdlog::error("xtp position query error: {}", (error_info ? error_info->error_msg : "unknown"));
        }
    }
}

void xtp_trader::req_qry_order()
{
    if (_td_api && _session_id > 0)
    {
        XTPQueryOrderReq req{};
        const int ret = _td_api->QueryOrders(&req, _session_id, next_request_id());
        if (ret != 0)
        {
            auto *error_info = _td_api->GetApiLastError();
            spdlog::error("xtp order query error: {}", (error_info ? error_info->error_msg : "unknown"));
        }
    }
}

void xtp_trader::OnDisconnected(uint64_t session_id, int reason)
{
    spdlog::warn("xtp td disconnected, reason={}", reason);
    _is_ready.store(false);
}

void xtp_trader::OnError(XTPRI *error_info)
{
    if (error_info && error_info->error_id != 0)
    {
        spdlog::error("xtp td error: {}", error_info->error_msg);
    }
}

void xtp_trader::OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id)
{
    if (!order_info)
    {
        return;
    }

    Order fresh = make_order_from_xtp(*order_info);
    const auto it = _order_map.find(fresh.order_ref);
    const int cached_traded = it != _order_map.end() ? it->second.volume_traded : 0;
    bump_order_ref(_order_ref, fresh.order_ref);

    if (has_error(error_info))
    {
        fresh.event_flag = eEventFlag::ErrorInsert;
        fresh.error_id = error_info->error_id;
        copy_text(fresh.error_msg, error_info->error_msg);
        _order_map[fresh.order_ref] = fresh;
        insert_event(fresh);
        return;
    }

    if (fresh.order_status == eOrderStatus::Canceled)
    {
        if (fresh.volume_traded > cached_traded)
        {
            Order trade_event = fresh;
            trade_event.event_flag = eEventFlag::Trade;
            _order_map[trade_event.order_ref] = trade_event;
            insert_event(trade_event);
        }

        fresh.event_flag = eEventFlag::Cancel;
        _order_map[fresh.order_ref] = fresh;
        insert_event(fresh);
        return;
    }

    if (fresh.volume_traded > cached_traded)
    {
        fresh.event_flag = eEventFlag::Trade;
    } else
    {
        fresh.volume_traded = cached_traded;
        fresh.event_flag = eEventFlag::Order;
    }

    _order_map[fresh.order_ref] = fresh;
    insert_event(fresh);
}

void xtp_trader::OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id)
{
    if (!trade_info)
    {
        return;
    }

    auto it = _order_map.find(trade_info->order_client_id);
    Order o = it != _order_map.end() ? it->second : make_trade_order(*trade_info);
    bump_order_ref(_order_ref, trade_info->order_client_id);

    const int previous_traded = o.volume_traded;
    int next_traded = previous_traded + static_cast<int>(trade_info->quantity);
    if (o.volume_total_original > 0)
    {
        next_traded = std::min(next_traded, o.volume_total_original);
        o.volume_total = std::max(0, o.volume_total_original - next_traded);
    } else
    {
        o.volume_total = std::max(0, o.volume_total - static_cast<int>(trade_info->quantity));
    }

    if (next_traded <= previous_traded)
    {
        return;
    }

    o.volume_traded = next_traded;
    o.order_status = o.volume_total == 0 ? eOrderStatus::AllTraded : eOrderStatus::PartTradedQueueing;
    o.event_flag = eEventFlag::Trade;

    _order_map[o.order_ref] = o;
    insert_event(o);
}

void xtp_trader::OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id)
{
    if (!cancel_info)
    {
        return;
    }

    const orderref_t order_ref = find_order_ref_by_xtp_id(_order_map, cancel_info->order_xtp_id);
    Order o{};
    if (order_ref != null_orderref)
    {
        o = _order_map[order_ref];
    }

    o.order_ref = order_ref;
    std::snprintf(o.order_sys_id, sizeof(o.order_sys_id), "%llu",
                  static_cast<unsigned long long>(cancel_info->order_xtp_id));
    o.event_flag = eEventFlag::ErrorCancel;
    if (has_error(error_info))
    {
        o.error_id = error_info->error_id;
        copy_text(o.error_msg, error_info->error_msg);
    }

    insert_event(o);
}

void xtp_trader::OnQueryPosition(XTPQueryStkPositionRsp *position, XTPRI *error_info, int request_id, bool is_last,
                                 uint64_t session_id)
{
    if (has_error(error_info))
    {
        spdlog::error("xtp position query callback error: {}", error_info->error_msg);
        return;
    }

    if (!position)
    {
        return;
    }

    Position &p = _position_map[position->ticker];
    p.id = position->ticker;

    if (position->position_direction == XTP_POSITION_DIRECTION_SHORT)
    {
        p.short_.position = static_cast<int>(position->total_qty);
        p.short_.closeable = static_cast<int>(position->sellable_qty);
        p.short_.avg_posi_cost = position->avg_price;
        p.short_.his_position = static_cast<int>(position->yesterday_position);
        p.short_.today_position = static_cast<int>(position->total_qty - position->yesterday_position);
        p.short_.his_closeable = std::min(p.short_.closeable, p.short_.his_position);
        p.short_.today_closeable = std::max(0, p.short_.closeable - p.short_.his_closeable);
    } else
    {
        p.long_.position = static_cast<int>(position->total_qty);
        p.long_.closeable = static_cast<int>(position->sellable_qty);
        p.long_.avg_posi_cost = position->avg_price;
        p.long_.his_position = static_cast<int>(position->yesterday_position);
        p.long_.today_position = static_cast<int>(position->total_qty - position->yesterday_position);
        p.long_.his_closeable = std::min(p.long_.closeable, p.long_.his_position);
        p.long_.today_closeable = std::max(0, p.long_.closeable - p.long_.his_closeable);
    }
}

void xtp_trader::OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id)
{
    if (has_error(error_info))
    {
        spdlog::error("xtp asset query callback error: {}", error_info->error_msg);
    }
}

void xtp_trader::OnQueryOrder(XTPQueryOrderRsp *order_info, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id)
{
    if (has_error(error_info))
    {
        spdlog::error("xtp order query callback error: {}", error_info->error_msg);
        return;
    }

    if (!order_info)
    {
        return;
    }

    Order order = make_order_from_xtp(*order_info);
    _order_map[order.order_ref] = order;
    bump_order_ref(_order_ref, order.order_ref);
}

void xtp_trader::OnQueryOrderEx(XTPOrderInfoEx *order_info, XTPRI *error_info, int request_id, bool is_last,
                                uint64_t session_id)
{
    if (has_error(error_info))
    {
        spdlog::error("xtp order query callback error: {}", error_info->error_msg);
        return;
    }

    if (!order_info)
    {
        return;
    }

    Order order = make_order_from_xtp(*order_info);
    _order_map[order.order_ref] = order;
    bump_order_ref(_order_ref, order.order_ref);
}
