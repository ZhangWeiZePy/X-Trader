#include "tora_trader.h"
#include <iostream>
#include <filesystem>
#include <cstring>
#include <chrono>

static inline void to_tora_dirOffset(eDirOffset dir_offset, TORASTOCKAPI::CTORATstpInputOrderField &t) {
    switch (dir_offset) {
        case eDirOffset::BuyOpen:
        case eDirOffset::BuyClose:
        case eDirOffset::BuyCloseToday:
        case eDirOffset::BuyCloseYesterday:
            t.Direction = TORASTOCKAPI::TORA_TSTP_D_Buy;
            break;
        case eDirOffset::SellOpen:
        case eDirOffset::SellClose:
        case eDirOffset::SellCloseToday:
        case eDirOffset::SellCloseYesterday:
            t.Direction = TORASTOCKAPI::TORA_TSTP_D_Sell;
            break;
    }
}

tora_trader::tora_trader(std::map<std::string, std::string> &config,
                         std::set<std::string> &contracts) : _contracts(contracts), _td_api(nullptr), _order_ref(1),
                                                             _session_id(0) {
    if (config.find("counter") == config.end() || config["counter"].empty()) {
        throw std::runtime_error("Missing or empty 'counter' in config");
    }
    std::string counter = config["counter"];
#ifdef _WIN32
    std::string lib_path = "lib/" + counter + "/traderapi.dll";
    const char* creator_name = "?CreateTstpTraderApi@CTORATstpTraderApi@TORASTOCKAPI@@SAPEAV12@PEBD_ND01@Z";
#else
    std::string lib_path = "lib/" + counter + "/libtraderapi.so";
    const char *creator_name = "_ZN12TORASTOCKAPI18CTORATstpTraderApi19CreateTstpTraderApiEPKcbcS2_b";
#endif

    if (!_loader.load(lib_path)) {
        throw std::runtime_error("Failed to load library: " + lib_path + ", error: " + _loader.get_error());
    }
    typedef TORASTOCKAPI::CTORATstpTraderApi * (*CreateTstpTraderApi_t)(
        const char *, bool, TORASTOCKAPI::TTORATstpTradeCommModeType, const char *, bool);
    CreateTstpTraderApi_t creator = _loader.get_function<CreateTstpTraderApi_t>(creator_name);
    if (!creator) {
        throw std::runtime_error("Failed to find symbol CreateTstpTraderApi in " + lib_path);
    }

    _user_id = config["user_id"];
    _password = config["password"];
    _front_addr = config["trade_front"]; // format like "tcp://127.0.0.1:8000"
    _department_id = config.count("department_id") ? config["department_id"] : "";
    _shareholder_id_sh = config.count("shareholder_id_sh") ? config["shareholder_id_sh"] : "";
    _shareholder_id_sz = config.count("shareholder_id_sz") ? config["shareholder_id_sz"] : "";

    char flow_path[64]{};
    sprintf(flow_path, "flow/tora/td/%s/", _user_id.c_str());
    if (!std::filesystem::exists(flow_path)) {
        std::filesystem::create_directories(flow_path);
    }

    _td_api = creator(flow_path, false, TORASTOCKAPI::TORA_TSTP_TCM_TCP, "", false);
    if (!_td_api) {
        throw std::runtime_error("Failed to create TORA Trader API");
    }

    _td_api->RegisterSpi(this);

    char front_addr[128]{};
    strcpy(front_addr, _front_addr.c_str());
    _td_api->RegisterFront(front_addr);
    _td_api->SubscribePrivateTopic(TORASTOCKAPI::TORA_TERT_QUICK);
    _td_api->SubscribePublicTopic(TORASTOCKAPI::TORA_TERT_QUICK);

    _td_api->Init();
}

tora_trader::~tora_trader() {
    release();
}

void tora_trader::release() {
    _is_ready.store(false);
    if (_td_api) {
        _td_api->RegisterSpi(nullptr);
        _td_api->Release();
        _td_api = nullptr;
    }
}

std::string tora_trader::get_trading_day() const
{
    // Tora API doesn't provide GetTradingDay() directly in CTORATstpTraderApi
    // You can usually get it from OnRtnOrder/OnRtnTrade/etc or just use system time if not available.
    return "";
}

void tora_trader::get_account() {
    if (_td_api) {
        TORASTOCKAPI::CTORATstpQryTradingAccountField req{};
        _td_api->ReqQryTradingAccount(&req, 0);
    }
}

void tora_trader::get_trader_data(InstrumentMap &i_map, PositionMap &p_map, OrderMap &o_map) {
    i_map = _instrument_map;
    p_map = _position_map;
    o_map = _order_map;
}

orderref_t tora_trader::insert_order(eOrderFlag order_flag, const std::string &contract, eDirOffset dir_offset,
                                     double price, int volume) {
    TORASTOCKAPI::CTORATstpInputOrderField req{};

    strcpy(req.SecurityID, contract.c_str());
    req.VolumeTotalOriginal = volume;
    req.LimitPrice = price;

    if (contract.length() > 0 && (contract[0] == '6' || contract[0] == '5')) {
        req.ExchangeID = TORASTOCKAPI::TORA_TSTP_EXD_SSE;
        strcpy(req.ShareholderID, _shareholder_id_sh.c_str());
    } else {
        req.ExchangeID = TORASTOCKAPI::TORA_TSTP_EXD_SZSE;
        strcpy(req.ShareholderID, _shareholder_id_sz.c_str());
    }

    to_tora_dirOffset(dir_offset, req);

    switch (order_flag) {
        case eOrderFlag::Limit:
            req.OrderPriceType = TORASTOCKAPI::TORA_TSTP_OPT_LimitPrice;
            break;
        case eOrderFlag::Market:
            req.OrderPriceType = TORASTOCKAPI::TORA_TSTP_OPT_AnyPrice;
            break;
        // case eOrderFlag::FOK:
        //     req.OrderPriceType = TORASTOCKAPI::TORA_TSTP_OPT_AnyPrice;
        //     req.TimeCondition = TORASTOCKAPI::TORA_TSTP_TC_IOC;
        //     break;
        // case eOrderFlag::FAK:
        //     req.OrderPriceType = TORASTOCKAPI::TORA_TSTP_OPT_AnyPrice;
        //     req.TimeCondition = TORASTOCKAPI::TORA_TSTP_TC_IOC;
        //     break;
    }

    if (req.TimeCondition == 0) {
        req.TimeCondition = TORASTOCKAPI::TORA_TSTP_TC_GFD;
    }
    req.VolumeCondition = TORASTOCKAPI::TORA_TSTP_VC_AV;

    req.OrderRef = _order_ref.fetch_add(1);

    int ret = _td_api->ReqOrderInsert(&req, 0);
    if (ret != 0) {
        return null_orderref;
    }
    return req.OrderRef;
}

bool tora_trader::cancel_order(const orderref_t order_ref) {
    const auto it = _order_map.find(order_ref);
    if (it == _order_map.end()) {
        return false;
    }
    auto &order = it->second;

    TORASTOCKAPI::CTORATstpInputOrderActionField req{};
    req.OrderActionRef = _order_ref.fetch_add(1);
    req.OrderRef = order.order_ref;
    req.FrontID = order.front_id;
    req.SessionID = order.session_id;
    req.ActionFlag = TORASTOCKAPI::TORA_TSTP_AF_Delete;

    // Tora needs ExchangeID and OrderSysID to cancel order accurately
    if (strlen(order.order_sys_id) > 0) {
        strcpy(req.OrderSysID, order.order_sys_id);
        req.ExchangeID = (order.instrument_id[0] == '6' || order.instrument_id[0] == '5')
                             ? TORASTOCKAPI::TORA_TSTP_EXD_SSE
                             : TORASTOCKAPI::TORA_TSTP_EXD_SZSE;
    }

    int ret = _td_api->ReqOrderAction(&req, 0);
    return ret == 0;
}

void tora_trader::req_qry_position() {
    if (_td_api) {
        TORASTOCKAPI::CTORATstpQryPositionField req{};
        _td_api->ReqQryPosition(&req, 0);
    }
}

void tora_trader::req_qry_order() {
    if (_td_api) {
        TORASTOCKAPI::CTORATstpQryOrderField req{};
        _td_api->ReqQryOrder(&req, 0);
    }
}

void tora_trader::OnFrontConnected() {
    std::cout << "tora td front connected" << std::endl;
    TORASTOCKAPI::CTORATstpReqUserLoginField req{};
    strcpy(req.LogInAccount, _user_id.c_str());
    req.LogInAccountType = TORASTOCKAPI::TORA_TSTP_LACT_UserID;
    strcpy(req.Password, _password.c_str());
    if (!_department_id.empty()) {
        strcpy(req.DepartmentID, _department_id.c_str());
    }
    _td_api->ReqUserLogin(&req, 1);
}

void tora_trader::OnFrontDisconnected(int nReason) {
    std::cout << "tora td disconnected, reason=" << nReason << std::endl;
    _is_ready.store(false);
}

void tora_trader::OnRspUserLogin(TORASTOCKAPI::CTORATstpRspUserLoginField *pRspUserLoginField,
                                 TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) {
    if (pRspInfoField && pRspInfoField->ErrorID != 0) {
        std::cout << "tora td login error: " << pRspInfoField->ErrorMsg << std::endl;
    } else {
        std::cout << "tora td login success!" << std::endl;
        if (pRspUserLoginField) {
            _session_id = pRspUserLoginField->SessionID;
        }
        req_qry_position();
        req_qry_order();
        _is_ready.store(true, std::memory_order_release);
    }
}

void tora_trader::OnRspOrderInsert(TORASTOCKAPI::CTORATstpInputOrderField *pInputOrderField,
                                   TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) {
    // implementation
}

void tora_trader::OnRtnOrder(TORASTOCKAPI::CTORATstpOrderField *pOrderField) {
    if (!pOrderField) return;

    Order o{};
    o.order_ref = pOrderField->OrderRef;
    o.front_id = pOrderField->FrontID;
    o.session_id = pOrderField->SessionID;
    strcpy(o.instrument_id, pOrderField->SecurityID);
    o.limit_price = pOrderField->LimitPrice;
    o.volume_total_original = pOrderField->VolumeTotalOriginal;
    o.volume_traded = pOrderField->VolumeTraded;
    o.volume_total = pOrderField->VolumeTotalOriginal - pOrderField->VolumeTraded;
    strcpy(o.order_sys_id, pOrderField->OrderSysID);

    if (pOrderField->Direction == TORASTOCKAPI::TORA_TSTP_D_Buy) {
        o.dir_offset = eDirOffset::BuyOpen;
    } else {
        o.dir_offset = eDirOffset::SellClose;
    }

    if (pOrderField->OrderStatus == TORASTOCKAPI::TORA_TSTP_OST_AllTraded) {
        o.order_status = eOrderStatus::AllTraded;
    } else if (pOrderField->OrderStatus == TORASTOCKAPI::TORA_TSTP_OST_AllCanceled || pOrderField->OrderStatus ==
               TORASTOCKAPI::TORA_TSTP_OST_PartTradeCanceled || pOrderField->OrderStatus ==
               TORASTOCKAPI::TORA_TSTP_OST_Rejected) {
        o.order_status = eOrderStatus::Canceled;
    } else if (pOrderField->OrderStatus == TORASTOCKAPI::TORA_TSTP_OST_PartTraded) {
        o.order_status = eOrderStatus::PartTradedQueueing;
    } else if (pOrderField->OrderStatus == TORASTOCKAPI::TORA_TSTP_OST_Accepted) {
        o.order_status = eOrderStatus::NoTradeQueueing;
    } else {
        o.order_status = eOrderStatus::Unknown;
    }

    o.event_flag = eEventFlag::Order;
    _order_map[o.order_ref] = o;
    this->insert_event(o);
}

void tora_trader::OnErrRtnOrderInsert(TORASTOCKAPI::CTORATstpInputOrderField *pInputOrderField,
                                      TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) {
    if (!pInputOrderField) return;
    Order o{};
    o.order_ref = pInputOrderField->OrderRef;
    o.event_flag = eEventFlag::ErrorInsert;
    if (pRspInfoField) {
        o.error_id = pRspInfoField->ErrorID;
        strcpy(o.error_msg, pRspInfoField->ErrorMsg);
    }
    this->insert_event(o);
}

void tora_trader::OnRtnTrade(TORASTOCKAPI::CTORATstpTradeField *pTradeField) {
    if (!pTradeField) return;
    Order o = _order_map[pTradeField->OrderRef];
    o.volume_traded += pTradeField->Volume;
    o.volume_total -= pTradeField->Volume;
    o.event_flag = eEventFlag::Trade;

    _order_map[o.order_ref] = o;
    this->insert_event(o);
}

void tora_trader::OnRspOrderAction(TORASTOCKAPI::CTORATstpInputOrderActionField *pInputOrderActionField,
                                   TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) {
    // implementation
}

void tora_trader::OnErrRtnOrderAction(TORASTOCKAPI::CTORATstpInputOrderActionField *pInputOrderActionField,
                                      TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID) {
    if (!pInputOrderActionField) return;
    Order o{};
    o.order_ref = pInputOrderActionField->OrderRef;
    o.event_flag = eEventFlag::ErrorCancel;
    if (pRspInfoField) {
        o.error_id = pRspInfoField->ErrorID;
        strcpy(o.error_msg, pRspInfoField->ErrorMsg);
    }
    this->insert_event(o);
}

void tora_trader::OnRspQryPosition(TORASTOCKAPI::CTORATstpPositionField *pPositionField,
                                   TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID, bool bIsLast) {
    if (pPositionField) {
        Position pos{};
        pos.id = pPositionField->SecurityID;
        // In A-shares, typically just mapping total volume to position and sellable
        // Simplification:
        pos.long_.position = pPositionField->CurrentPosition;
        pos.long_.closeable = pPositionField->AvailablePosition;
        _position_map[pos.id] = pos;
    }
}

void tora_trader::OnRspQryOrder(TORASTOCKAPI::CTORATstpOrderField *pOrderField,
                                TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID, bool bIsLast) {
    if (pOrderField) {
        OnRtnOrder(pOrderField);
    }
}

void tora_trader::OnRspQryTradingAccount(TORASTOCKAPI::CTORATstpTradingAccountField *pTradingAccountField,
                                         TORASTOCKAPI::CTORATstpRspInfoField *pRspInfoField, int nRequestID,
                                         bool bIsLast) {
}
