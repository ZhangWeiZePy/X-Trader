#include "xtp_market.h"
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <cstring>

namespace
{
    inline bool is_bid_side(const char side)
    {
        return side == 'B' || side == '1';
    }

    inline bool is_ask_side(const char side)
    {
        return side == 'S' || side == '2';
    }

    template<typename BookType>
    bool add_level_qty(BookType &book, const double price, const int64_t qty)
    {
        if (price <= 0.0 || qty <= 0)
        {
            return false;
        }
        auto it = book.find(price);
        if (it == book.end())
        {
            book[price] = qty;
            return true;
        }
        it->second += qty;
        return true;
    }

    template<typename BookType>
    bool reduce_level_qty(BookType &book, const double price, const int64_t qty)
    {
        if (price <= 0.0 || qty <= 0)
        {
            return false;
        }
        auto it = book.find(price);
        if (it == book.end())
        {
            return false;
        }
        if (it->second <= qty)
        {
            book.erase(it);
            return true;
        }
        it->second -= qty;
        return true;
    }
}

xtp_market::xtp_market(std::map<std::string, std::string> &config, std::set<std::string> &contracts) :
    _contracts(contracts), _md_api(nullptr)
{
    _local_books.reserve(_contracts.size() * 2 + 8);
    _previous_tick_map.reserve(_contracts.size() * 2 + 8);
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
    _cfg.user_id = config["user_id"];
    _cfg.password = config["password"];
    _cfg.client_id = config.count("client_id") ? std::stoi(config["client_id"]) : 1;
    printf("xtp market client_id: %d\n", _cfg.client_id);
    std::string front = config["market_front"]; // format like "tcp://127.0.0.1:8000" or "127.0.0.1:8000"
    size_t pos = front.find("://");
    if (pos != std::string::npos)
    {
        front = front.substr(pos + 3);
    }
    pos = front.find(':');
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

    char flow_path[64]{};
    sprintf(flow_path, "flow/xtp/md/%s/", _cfg.user_id.c_str());
    if (!std::filesystem::exists(flow_path))
    {
        std::filesystem::create_directories(flow_path);
    }

    _md_api = creator(_cfg.client_id, flow_path, XTP_LOG_LEVEL_INFO);
    if (!_md_api)
    {
        throw std::runtime_error("Failed to create XTP Quote API");
    }

    _md_api->SetHeartBeatInterval(15);
    _md_api->RegisterSpi(this);

    int ret = _md_api->Login(_cfg.server_ip.c_str(), _cfg.server_port, _cfg.user_id.c_str(),
                             _cfg.password.c_str(), _cfg.protocol_type);
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

void xtp_market::init_book_from_depth(const XTPMD &md)
{
    auto &book = _local_books[md.ticker];
    book.bids.clear();
    book.asks.clear();
    book.order_ref_index.clear();
    book.channel_seq_state.clear();
    book.order_ref_index.reserve(4096);
    book.channel_seq_state.reserve(64);
    for (int i = 0; i < 10; ++i)
    {
        if (md.bid[i] > 0.0 && md.bid_qty[i] > 0)
        {
            book.bids[md.bid[i]] = md.bid_qty[i];
        }
        if (md.ask[i] > 0.0 && md.ask_qty[i] > 0)
        {
            book.asks[md.ask[i]] = md.ask_qty[i];
        }
    }
    book.initialized = true;
    book.valid = true;
    book.last_price = md.last_price;
    book.qty = md.qty;
    book.turnover = md.turnover;
    book.trades_count = md.trades_count;
    book.data_time = md.data_time;
    memset(&book.ob_cache, 0, sizeof(book.ob_cache));
    book.ob_cache.exchange_id = static_cast<int32_t>(md.exchange_id);
    strcpy(book.ob_cache.instrument_id, md.ticker);
    refresh_top10_bid(book);
    refresh_top10_ask(book);
    emit_book(book, md.exchange_id, md.data_time, kBookUpdateBid | kBookUpdateAsk | kBookUpdateStat);
}

uint8_t xtp_market::apply_tbt_entrust(const XTPTBT &tbt)
{
    auto it = _local_books.find(tbt.ticker);
    if (it == _local_books.end() || !it->second.initialized || !it->second.valid)
    {
        return kBookUpdateNone;
    }
    auto &book = it->second;
    const auto &entrust = tbt.entrust;
    book.data_time = tbt.data_time;

    const bool is_bid = is_bid_side(entrust.side);
    const bool is_ask = is_ask_side(entrust.side);
    if (!is_bid && !is_ask)
    {
        return kBookUpdateNone;
    }

    bool changed = false;
    uint8_t mask = kBookUpdateNone;
    if (tbt.exchange_id == XTP_EXCHANGE_SH)
    {
        if (entrust.ord_type == 'A')
        {
            if (is_bid)
            {
                changed = add_level_qty(book.bids, entrust.price, entrust.qty);
            } else if (is_ask)
            {
                changed = add_level_qty(book.asks, entrust.price, entrust.qty);
            }
            if (changed && entrust.order_no > 0)
            {
                book.order_ref_index[entrust.order_no] = OrderRefMeta{entrust.price, entrust.qty, is_bid};
            }
        } else if (entrust.ord_type == 'D')
        {
            if (entrust.order_no > 0)
            {
                auto idx_it = book.order_ref_index.find(entrust.order_no);
                if (idx_it != book.order_ref_index.end())
                {
                    auto &meta = idx_it->second;
                    if (meta.is_bid)
                    {
                        changed = reduce_level_qty(book.bids, meta.price, meta.qty);
                        if (changed)
                        {
                            mask |= kBookUpdateBid;
                        }
                    } else
                    {
                        changed = reduce_level_qty(book.asks, meta.price, meta.qty);
                        if (changed)
                        {
                            mask |= kBookUpdateAsk;
                        }
                    }
                    book.order_ref_index.erase(idx_it);
                } else
                {
                    if (is_bid)
                    {
                        changed = reduce_level_qty(book.bids, entrust.price, entrust.qty);
                        if (changed)
                        {
                            mask |= kBookUpdateBid;
                        }
                    } else if (is_ask)
                    {
                        changed = reduce_level_qty(book.asks, entrust.price, entrust.qty);
                        if (changed)
                        {
                            mask |= kBookUpdateAsk;
                        }
                    }
                }
            } else
            {
                if (is_bid)
                {
                    changed = reduce_level_qty(book.bids, entrust.price, entrust.qty);
                    if (changed)
                    {
                        mask |= kBookUpdateBid;
                    }
                } else if (is_ask)
                {
                    changed = reduce_level_qty(book.asks, entrust.price, entrust.qty);
                    if (changed)
                    {
                        mask |= kBookUpdateAsk;
                    }
                }
            }
        }
    } else
    {
        if (is_bid)
        {
            changed = add_level_qty(book.bids, entrust.price, entrust.qty);
            if (changed)
            {
                mask |= kBookUpdateBid;
            }
        } else if (is_ask)
        {
            changed = add_level_qty(book.asks, entrust.price, entrust.qty);
            if (changed)
            {
                mask |= kBookUpdateAsk;
            }
        }
    }
    if (changed)
    {
        return mask;
    }
    return kBookUpdateNone;
}

uint8_t xtp_market::apply_tbt_trade(const XTPTBT &tbt)
{
    auto it = _local_books.find(tbt.ticker);
    if (it == _local_books.end() || !it->second.initialized || !it->second.valid)
    {
        return kBookUpdateNone;
    }
    auto &book = it->second;
    const auto &trade = tbt.trade;
    book.data_time = tbt.data_time;
    bool changed = false;
    uint8_t mask = kBookUpdateNone;
    const bool is_sz_cancel = (tbt.exchange_id == XTP_EXCHANGE_SZ && trade.trade_flag == '4');
    if (!is_sz_cancel)
    {
        // 仅在有效成交记录时更新成交统计，避免撤单价(常为0)污染last_price。
        if (trade.price > 0.0)
        {
            book.last_price = trade.price;
            changed = true;
        }
        if (trade.qty > 0)
        {
            book.qty += trade.qty;
            changed = true;
        }
        if (trade.money > 0.0)
        {
            book.turnover += trade.money;
            changed = true;
        } else if (trade.price > 0.0 && trade.qty > 0)
        {
            book.turnover += trade.price * static_cast<double>(trade.qty);
            changed = true;
        }
        ++book.trades_count;
        changed = true;
        mask |= kBookUpdateStat;
    }
    auto reduce_with_order_ref = [&](const int64_t order_no) -> bool
    {
        if (order_no <= 0)
        {
            return false;
        }
        auto idx_it = book.order_ref_index.find(order_no);
        if (idx_it == book.order_ref_index.end())
        {
            return false;
        }
        auto &meta = idx_it->second;
        const int64_t reduce_qty = std::min<int64_t>(trade.qty, meta.qty);
        bool reduced = false;
        if (meta.is_bid)
        {
            reduced = reduce_level_qty(book.bids, meta.price, reduce_qty);
            if (reduced)
            {
                mask |= kBookUpdateBid;
            }
        } else
        {
            reduced = reduce_level_qty(book.asks, meta.price, reduce_qty);
            if (reduced)
            {
                mask |= kBookUpdateAsk;
            }
        }
        if (reduce_qty >= meta.qty)
        {
            book.order_ref_index.erase(idx_it);
        } else
        {
            meta.qty -= reduce_qty;
        }
        return reduced;
    };

    if (tbt.exchange_id == XTP_EXCHANGE_SH)
    {
        bool reduced = false;
        reduced = reduce_with_order_ref(trade.bid_no) || reduced;
        reduced = reduce_with_order_ref(trade.ask_no) || reduced;
        if (!reduced)
        {
            if (trade.trade_flag == 'B')
            {
                reduced = reduce_level_qty(book.asks, trade.price, trade.qty);
                if (reduced)
                {
                    mask |= kBookUpdateAsk;
                }
            } else if (trade.trade_flag == 'S')
            {
                reduced = reduce_level_qty(book.bids, trade.price, trade.qty);
                if (reduced)
                {
                    mask |= kBookUpdateBid;
                }
            }
        }
        if (reduced)
        {
            changed = true;
        }
    } else
    {
        if (trade.trade_flag == 'F' || trade.trade_flag == '4')
        {
            bool reduced = reduce_level_qty(book.asks, trade.price, trade.qty);
            if (reduced)
            {
                mask |= kBookUpdateAsk;
            }
            if (!reduced)
            {
                reduced = reduce_level_qty(book.bids, trade.price, trade.qty);
                if (reduced)
                {
                    mask |= kBookUpdateBid;
                }
            }
            if (reduced)
            {
                changed = true;
            }
        }
    }
    if (changed)
    {
        return mask;
    }
    return kBookUpdateNone;
}

void xtp_market::refresh_top10_bid(LocalBookState &book)
{
    memset(book.ob_cache.bid, 0, sizeof(book.ob_cache.bid));
    memset(book.ob_cache.bid_qty, 0, sizeof(book.ob_cache.bid_qty));
    int level = 0;
    for (const auto &kv: book.bids)
    {
        if (level >= 10)
        {
            break;
        }
        book.ob_cache.bid[level] = kv.first;
        book.ob_cache.bid_qty[level] = kv.second;
        ++level;
    }
}

void xtp_market::refresh_top10_ask(LocalBookState &book)
{
    memset(book.ob_cache.ask, 0, sizeof(book.ob_cache.ask));
    memset(book.ob_cache.ask_qty, 0, sizeof(book.ob_cache.ask_qty));
    int level = 0;
    for (const auto &kv: book.asks)
    {
        if (level >= 10)
        {
            break;
        }
        book.ob_cache.ask[level] = kv.first;
        book.ob_cache.ask_qty[level] = kv.second;
        ++level;
    }
}

void xtp_market::emit_book(LocalBookState &book, XTP_EXCHANGE_TYPE exchange_id, int64_t data_time, uint8_t update_mask)
{
    if (update_mask & kBookUpdateBid)
    {
        refresh_top10_bid(book);
    }
    if (update_mask & kBookUpdateAsk)
    {
        refresh_top10_ask(book);
    }
    if (update_mask & kBookUpdateStat)
    {
        book.ob_cache.last_price = book.last_price;
        book.ob_cache.qty = book.qty;
        book.ob_cache.turnover = book.turnover;
        book.ob_cache.trades_count = book.trades_count;
    }
    book.ob_cache.exchange_id = static_cast<int32_t>(exchange_id);
    book.ob_cache.data_time = data_time;
    this->insert_event(book.ob_cache);
    emit_orderbook(book.ob_cache);
}

void xtp_market::mark_book_invalid_until_snapshot(LocalBookState &book)
{
    book.valid = false;
    book.order_ref_index.clear();
    book.channel_seq_state.clear();
}

bool xtp_market::check_and_update_seq(LocalBookState &book, int32_t channel_no, int64_t seq)
{
    if (channel_no <= 0 || seq <= 0)
    {
        return true;
    }
    auto it = book.channel_seq_state.find(channel_no);
    if (it == book.channel_seq_state.end())
    {
        book.channel_seq_state[channel_no] = seq;
        return true;
    }
    const int64_t expected = it->second + 1;
    if (seq != expected)
    {
        return false;
    }
    it->second = seq;
    return true;
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
    auto local_it = _local_books.find(ptr->ticker);
    if (local_it == _local_books.end() || !local_it->second.initialized || !local_it->second.valid)
    {
        init_book_from_depth(*ptr);
    } else
    {
        if (ptr->last_price > 0.0)
        {
            local_it->second.last_price = ptr->last_price;
        }
        local_it->second.qty = ptr->qty;
        local_it->second.turnover = ptr->turnover;
        local_it->second.trades_count = ptr->trades_count;
        local_it->second.data_time = ptr->data_time;
        emit_book(local_it->second, ptr->exchange_id, ptr->data_time, kBookUpdateStat);
    }
    _previous_tick_map[ptr->ticker] = *ptr;
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
    } else if (tbt_data->type == XTP_TBT_TRADE)
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
    } else
    {
        return;
    }

    auto local_it = _local_books.find(tbt_data->ticker);
    if (local_it == _local_books.end() || !local_it->second.initialized || !local_it->second.valid)
    {
        return;
    }

    int32_t channel_no = 0;
    int64_t seq = 0;
    if (tbt_data->type == XTP_TBT_ENTRUST)
    {
        channel_no = tbt_data->entrust.channel_no;
        seq = tbt_data->entrust.seq;
    } else if (tbt_data->type == XTP_TBT_TRADE)
    {
        channel_no = tbt_data->trade.channel_no;
        seq = tbt_data->trade.seq;
    }

    if (!check_and_update_seq(local_it->second, channel_no, seq))
    {
        mark_book_invalid_until_snapshot(local_it->second);
        return;
    }

    uint8_t update_mask = kBookUpdateNone;
    if (tbt_data->type == XTP_TBT_ENTRUST)
    {
        update_mask = apply_tbt_entrust(*tbt_data);
    } else if (tbt_data->type == XTP_TBT_TRADE)
    {
        update_mask = apply_tbt_trade(*tbt_data);
    }

    if (update_mask != kBookUpdateNone)
    {
        emit_book(local_it->second, tbt_data->exchange_id, tbt_data->data_time, update_mask);
    }
}
