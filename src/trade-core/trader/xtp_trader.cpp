#include "xtp_trader.h"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <chrono>

static inline void to_xtp_dirOffset(eDirOffset dir_offset, XTPOrderInsertInfo &t)
{
    switch (dir_offset)
    {
        case eDirOffset::BuyOpen:
            t.side = XTP_SIDE_BUY;
            t.position_effect = XTP_POSITION_EFFECT_OPEN;
            break;
        case eDirOffset::SellOpen:
            t.side = XTP_SIDE_SELL;
            t.position_effect = XTP_POSITION_EFFECT_OPEN;
            break;
        case eDirOffset::BuyClose:
        case eDirOffset::BuyCloseToday:
        case eDirOffset::BuyCloseYesterday:
            t.side = XTP_SIDE_BUY;
            t.position_effect = XTP_POSITION_EFFECT_CLOSE;
            break;
        case eDirOffset::SellClose:
        case eDirOffset::SellCloseToday:
        case eDirOffset::SellCloseYesterday:
            t.side = XTP_SIDE_SELL;
            t.position_effect = XTP_POSITION_EFFECT_CLOSE;
            break;
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
    _user_id = config["user_id"];
    _password = config["password"];
    _client_id = config.count("client_id") ? std::stoi(config["client_id"]) : 1;
    std::string front = config["trade_front"];
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
    sprintf(flow_path, "flow/xtp/td/%s/", _user_id.c_str());
    if (!std::filesystem::exists(flow_path))
    {
        std::filesystem::create_directories(flow_path);
    }

    _td_api = creator(_client_id, flow_path, XTP_LOG_LEVEL_INFO);
    if (!_td_api)
    {
        throw std::runtime_error("Failed to create XTP Trader API");
    }

    _td_api->RegisterSpi(this);
    // Set heart beat to 15s
    _td_api->SetHeartBeatInterval(15);
    // In XTP, you should specify the software version
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

    _session_id = _td_api->Login(_server_ip.c_str(), _server_port, _user_id.c_str(), _password.c_str(), _protocol_type);
    if (_session_id == 0)
    {
        XTPRI *error_info = _td_api->GetApiLastError();
        std::cout << "xtp td login error: " << (error_info ? error_info->error_msg : "unknown") << std::endl;
    } else
    {
        std::cout << "xtp td login success! session_id=" << _session_id << std::endl;
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
        _td_api->QueryAsset(_session_id, 0);
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
    XTPOrderInsertInfo t{};
    t.order_client_id = _order_ref.fetch_add(1);
    strcpy(t.ticker, contract.c_str());

    if (contract.length() > 0 && (contract[0] == '6' || contract[0] == '5'))
    {
        t.market = XTP_MKT_SH_A;
    } else
    {
        t.market = XTP_MKT_SZ_A;
    }

    t.price = price;
    t.quantity = volume;

    to_xtp_dirOffset(dir_offset, t);

    switch (order_flag)
    {
        case eOrderFlag::Limit:
            t.price_type = XTP_PRICE_LIMIT;
            break;
        case eOrderFlag::Market:
            if (t.market == XTP_MKT_SH_A)
            {
                t.price_type = XTP_PRICE_BEST5_OR_LIMIT;
            } else
            {
                t.price_type = XTP_PRICE_BEST_OR_CANCEL;
            }
            break;
        case eOrderFlag::FOK:
            t.price_type = XTP_PRICE_ALL_OR_CANCEL;
            break;
        case eOrderFlag::FAK:
            t.price_type = XTP_PRICE_BEST5_OR_CANCEL;
            break;
    }

    t.business_type = XTP_BUSINESS_TYPE_CASH;

    uint64_t xtp_id = _td_api->InsertOrder(&t, _session_id);
    if (xtp_id == 0)
    {
        return null_orderref;
    }
    return t.order_client_id;
}

bool xtp_trader::cancel_order(const orderref_t order_ref)
{
    const auto it = _order_map.find(order_ref);
    if (it == _order_map.end())
    {
        return false;
    }
    auto &order = it->second;

    uint64_t order_xtp_id = 0; // we need order_xtp_id from OrderMap or we can cancel by order_client_id
    // wait, XTP cancel order requires order_xtp_id
    order_xtp_id = std::stoull(order.order_sys_id);

    uint64_t cancel_id = _td_api->CancelOrder(order_xtp_id, _session_id);
    return cancel_id != 0;
}

void xtp_trader::req_qry_position()
{
    if (_td_api && _session_id > 0)
    {
        _td_api->QueryPosition(nullptr, _session_id, 0);
    }
}

void xtp_trader::req_qry_order()
{
    if (_td_api && _session_id > 0)
    {
        XTPQueryOrderReq req{};
        _td_api->QueryOrders(&req, _session_id, 0);
    }
}

void xtp_trader::OnDisconnected(uint64_t session_id, int reason)
{
    std::cout << "xtp td disconnected, reason=" << reason << std::endl;
    _is_ready.store(false);
}

void xtp_trader::OnError(XTPRI *error_info)
{
    if (error_info && error_info->error_id != 0)
    {
        std::cout << "xtp td error: " << error_info->error_msg << std::endl;
    }
}

void xtp_trader::OnOrderEvent(XTPOrderInfo *order_info, XTPRI *error_info, uint64_t session_id)
{
    Order o{};
    o.order_ref = order_info->order_client_id;
    strcpy(o.instrument_id, order_info->ticker);
    o.limit_price = order_info->price;
    o.volume_total_original = order_info->quantity;
    o.volume_traded = order_info->qty_traded;
    o.volume_total = order_info->qty_left;
    sprintf(o.order_sys_id, "%llu", order_info->order_xtp_id);

    if (order_info->side == XTP_SIDE_BUY)
    {
        o.dir_offset = eDirOffset::BuyOpen;
    } else if (order_info->side == XTP_SIDE_SELL)
    {
        o.dir_offset = eDirOffset::SellClose;
    }

    if (order_info->order_status == XTP_ORDER_STATUS_ALLTRADED)
    {
        o.order_status = eOrderStatus::AllTraded;
    } else if (order_info->order_status == XTP_ORDER_STATUS_CANCELED)
    {
        o.order_status = eOrderStatus::Canceled;
    } else if (order_info->order_status == XTP_ORDER_STATUS_PARTTRADEDQUEUEING)
    {
        o.order_status = eOrderStatus::PartTradedQueueing;
    } else if (order_info->order_status == XTP_ORDER_STATUS_PARTTRADEDNOTQUEUEING)
    {
        o.order_status = eOrderStatus::PartTradedNotQueueing;
    } else if (order_info->order_status == XTP_ORDER_STATUS_NOTRADEQUEUEING)
    {
        o.order_status = eOrderStatus::NoTradeQueueing;
    } else if (order_info->order_status == XTP_ORDER_STATUS_REJECTED)
    {
        o.order_status = eOrderStatus::Canceled;
    }

    if (error_info && error_info->error_id != 0)
    {
        o.error_id = error_info->error_id;
        strcpy(o.error_msg, error_info->error_msg);
        o.event_flag = eEventFlag::ErrorInsert;
    } else
    {
        o.event_flag = eEventFlag::Order;
    }

    _order_map[o.order_ref] = o;
    this->insert_event(o);
}

void xtp_trader::OnTradeEvent(XTPTradeReport *trade_info, uint64_t session_id)
{
    Order o = _order_map[trade_info->order_client_id];
    o.volume_traded += trade_info->quantity;
    o.volume_total -= trade_info->quantity;
    o.event_flag = eEventFlag::Trade;

    _order_map[o.order_ref] = o;
    this->insert_event(o);
}

void xtp_trader::OnCancelOrderError(XTPOrderCancelInfo *cancel_info, XTPRI *error_info, uint64_t session_id)
{
    // find order
    for (auto &pair: _order_map)
    {
        if (std::stoull(pair.second.order_sys_id) == cancel_info->order_xtp_id)
        {
            Order o = pair.second;
            o.event_flag = eEventFlag::ErrorCancel;
            if (error_info)
            {
                o.error_id = error_info->error_id;
                strcpy(o.error_msg, error_info->error_msg);
            }
            this->insert_event(o);
            break;
        }
    }
}

void xtp_trader::OnQueryPosition(XTPQueryStkPositionRsp *position, XTPRI *error_info, int request_id, bool is_last,
                                 uint64_t session_id)
{
    if (position)
    {
        Position p{};
        p.id = position->ticker;
        p.long_.position = position->total_qty;
        p.long_.closeable = position->sellable_qty;
        p.long_.avg_posi_cost = position->avg_price;
        _position_map[position->ticker] = p;
    }
}

void xtp_trader::OnQueryAsset(XTPQueryAssetRsp *asset, XTPRI *error_info, int request_id, bool is_last,
                              uint64_t session_id)
{
    // asset details
}

void xtp_trader::OnQueryOrderEx(XTPOrderInfoEx *order_info, XTPRI *error_info, int request_id, bool is_last,
                                uint64_t session_id)
{
    if (order_info)
    {
        Order o{};
        o.order_ref = order_info->order_client_id;
        strcpy(o.instrument_id, order_info->ticker);
        o.limit_price = order_info->price;
        o.volume_total_original = order_info->quantity;
        o.volume_traded = order_info->qty_traded;
        o.volume_total = order_info->qty_left;
        sprintf(o.order_sys_id, "%llu", order_info->order_xtp_id);
        _order_map[o.order_ref] = o;
    }
}
