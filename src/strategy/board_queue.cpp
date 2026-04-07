#include "board_queue.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace
{
    // 去掉字符串首尾空白，避免配置值受空格影响
    std::string trim_copy(const std::string &s)
    {
        const auto begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return "";
        }
        const auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    std::string lower_copy(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c)
                       {
                           return static_cast<char>(std::tolower(c));
                       });
        return s;
    }

    // 解析布尔配置，兼容 1/0、true/false、yes/no、on/off
    bool parse_bool(const std::string &raw, bool &out)
    {
        const std::string v = lower_copy(trim_copy(raw));
        if (v == "1" || v == "true" || v == "yes" || v == "y" || v == "on")
        {
            out = true;
            return true;
        }
        if (v == "0" || v == "false" || v == "no" || v == "n" || v == "off")
        {
            out = false;
            return true;
        }
        return false;
    }

    // 校验时间格式是否为 HH:MM:SS，且时分秒范围合法
    bool is_hhmmss(const std::string &s)
    {
        if (s.size() != 8)
        {
            return false;
        }
        if (s[2] != ':' || s[5] != ':')
        {
            return false;
        }
        if (!std::isdigit(static_cast<unsigned char>(s[0])) || !std::isdigit(static_cast<unsigned char>(s[1])) ||
            !std::isdigit(static_cast<unsigned char>(s[3])) || !std::isdigit(static_cast<unsigned char>(s[4])) ||
            !std::isdigit(static_cast<unsigned char>(s[6])) || !std::isdigit(static_cast<unsigned char>(s[7])))
        {
            return false;
        }
        const int hh = std::stoi(s.substr(0, 2));
        const int mm = std::stoi(s.substr(3, 2));
        const int ss = std::stoi(s.substr(6, 2));
        if (hh < 0 || hh > 23)
        {
            return false;
        }
        if (mm < 0 || mm > 59)
        {
            return false;
        }
        if (ss < 0 || ss > 59)
        {
            return false;
        }
        return true;
    }

    // 当没有最新 tick 时间时，回退到本机当前时间用于超时判断
    std::string now_hhmmss()
    {
        const auto now = std::chrono::system_clock::now();
        const std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tmv{};
#if defined(_WIN32)
		localtime_s(&tmv, &tt);
#else
        localtime_r(&tt, &tmv);
#endif
        char buf[9] = {0};
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
        return std::string(buf);
    }
}

bool board_queue::set_config(const std::map<std::string, std::string> &config)
{
    // 先保存原始配置到基类
    if (!strategy::set_config(config))
    {
        return false;
    }
    // 合约是必填项，用于订阅与过滤行情
    const auto &contract = get_config("contract");
    if (contract.empty())
    {
        printf("board_queue config missing contract\n");
        return false;
    }
    // 注册策略关注合约
    _contract = contract;
    get_contracts().clear();
    get_contracts().insert(_contract);
    size_t key_w = 3;
    size_t val_w = 5;
    for (const auto &kv : config)
    {
        key_w = std::max(key_w, kv.first.size());
        val_w = std::max(val_w, kv.second.size());
    }
    std::string sep = "+-" + std::string(key_w, '-') + "-+-" + std::string(val_w, '-') + "-+\n";
    printf("%s", sep.c_str());
    printf("| %-*s | %-*s |\n", (int)key_w, "Key", (int)val_w, "Value");
    printf("%s", sep.c_str());
    for (const auto &kv : config)
    {
        printf("| %-*s | %-*s |\n", (int)key_w, kv.first.c_str(), (int)val_w, kv.second.c_str());
    }
    printf("%s", sep.c_str());
    return true;
}

void board_queue::on_init()
{
    // 启动时统一解析并校验所有参数，失败即不激活策略
    try
    {
        _contract = trim_copy(get_config("contract"));
        // 解析订单类型：limit / market
        const std::string order_type = lower_copy(trim_copy(get_config("order_type")));
        if (order_type == "limit")
        {
            _order_flag = eOrderFlag::Limit;
        } else if (order_type == "market")
        {
            _order_flag = eOrderFlag::Market;
        } else
        {
            printf("board_queue order_type must be limit or market\n");
            return;
        }

        // 解析下单数量，必须大于 0
        _quantity = std::stoi(trim_copy(get_config("quantity")));
        if (_quantity <= 0)
        {
            printf("board_queue quantity must be positive\n");
            return;
        }

        // 解析有效时间窗口，要求 start < end
        _active_start_time = trim_copy(get_config("active_start_time"));
        _active_end_time = trim_copy(get_config("active_end_time"));
        if (!is_hhmmss(_active_start_time) || !is_hhmmss(_active_end_time) || _active_start_time >= _active_end_time)
        {
            printf("board_queue active time invalid, expected HH:MM:SS and start < end\n");
            return;
        }

        // 解析四个条件开关（排板金额/手数、撤单金额/手数）
        if (!parse_bool(get_config("enable_queue_amount_enter"), _enable_queue_amount_enter) ||
            !parse_bool(get_config("enable_queue_lots_enter"), _enable_queue_lots_enter) ||
            !parse_bool(get_config("enable_queue_amount_exit"), _enable_queue_amount_exit) ||
            !parse_bool(get_config("enable_queue_lots_exit"), _enable_queue_lots_exit))
        {
            printf("board_queue enable flags invalid\n");
            return;
        }

        // 若启用“金额排板”，阈值必须为正数
        if (_enable_queue_amount_enter)
        {
            _queue_amount_enter = std::stod(trim_copy(get_config("queue_amount_enter")));
            if (_queue_amount_enter <= 0)
            {
                printf("board_queue queue_amount_enter must be positive\n");
                return;
            }
        }
        // 若启用“手数排板”，阈值必须为正数
        if (_enable_queue_lots_enter)
        {
            _queue_lots_enter = std::stod(trim_copy(get_config("queue_lots_enter")));
            if (_queue_lots_enter <= 0)
            {
                printf("board_queue queue_lots_enter must be positive\n");
                return;
            }
        }
        // 至少启用一个排板条件，避免策略无法触发
        if (!_enable_queue_amount_enter && !_enable_queue_lots_enter)
        {
            printf("board_queue at least one enter condition must be enabled\n");
            return;
        }

        // 若启用“金额撤单”，阈值必须为正数
        if (_enable_queue_amount_exit)
        {
            _queue_amount_exit = std::stod(trim_copy(get_config("queue_amount_exit")));
            if (_queue_amount_exit <= 0)
            {
                printf("board_queue queue_amount_exit must be positive\n");
                return;
            }
        }
        // 若启用“手数撤单”，阈值必须为正数
        if (_enable_queue_lots_exit)
        {
            _queue_lots_exit = std::stod(trim_copy(get_config("queue_lots_exit")));
            if (_queue_lots_exit <= 0)
            {
                printf("board_queue queue_lots_exit must be positive\n");
                return;
            }
        }

        // 全部参数通过校验，标记初始化完成
        _inited = true;
    } catch (...)
    {
        // 任意转换异常都视为配置错误
        printf("board_queue config parse failed\n");
    }
}

bool board_queue::in_active_window(const std::string &hhmmss) const
{
    // 仅在配置时间窗内允许触发和维持挂单
    return hhmmss >= _active_start_time && hhmmss <= _active_end_time;
}

inline bool board_queue::should_enter(double board_amount, double board_lots) const
{
    // 排板条件为“任一满足即可”
    bool by_amount = false;
    bool by_lots = false;
    if (_enable_queue_amount_enter)
    {
        by_amount = board_amount >= _queue_amount_enter;
    }
    if (_enable_queue_lots_enter)
    {
        by_lots = board_lots >= _queue_lots_enter;
    }
    return by_amount || by_lots;
}

inline bool board_queue::should_exit(double board_amount, double board_lots) const
{
    // 撤单条件为“任一满足即撤”
    bool by_amount = false;
    bool by_lots = false;
    if (_enable_queue_amount_exit)
    {
        by_amount = board_amount < _queue_amount_exit;
    }
    if (_enable_queue_lots_exit)
    {
        by_lots = board_lots < _queue_lots_exit;
    }
    return by_amount || by_lots;
}

inline bool board_queue::compute_board_metrics(const MarketData &tick, double &board_amount, double &board_lots) const
{
    // 取买一价量作为封板口径
    const double bid1_price = tick.bid_price[0];
    const int bid1_volume = tick.bid_volume[0];
    board_amount = 0.0;
    board_lots = 0.0;
    // 买一无量则不构成封板
    if (bid1_volume <= 0)
    {
        return false;
    }
    // 仅当买一价等于涨停价，才按封板处理
    if (std::fabs(bid1_price - tick.upper_limit_price) > 1e-6)
    {
        return false;
    }
    // 计算封板金额（元）与封板手数（手）
    board_amount = bid1_price * static_cast<double>(bid1_volume);
    board_lots = static_cast<double>(bid1_volume) / 100.0;
    return true;
}

void board_queue::on_tick(const MarketData &tick)
{
    // 初始化失败时不参与交易
    if (unlikely(!_inited))
    {
        return;
    }
    // 仅处理目标合约行情
    if (unlikely(_contract != tick.instrument_id))
    {
        return;
    }
    // 更新最新行情时间与封板指标缓存，供撤单判断复用
    _latest_tick_time = tick.update_time;
    double board_amount = 0.0;
    double board_lots = 0.0;
    if (compute_board_metrics(tick, board_amount, board_lots))
    {
        _latest_board_amount = board_amount;
        _latest_board_lots = board_lots;
    } else
    {
        _latest_board_amount = 0.0;
        _latest_board_lots = 0.0;
    }

    // 有活动单时不重复下单
    if (_active_orderref != null_orderref)
    {
        return;
    }
    // 仅首单排板：下过一次成功单后不再重挂
    if (_has_placed_once)
    {
        return;
    }
    // 不在有效时间内不下单
    if (!in_active_window(_latest_tick_time))
    {
        return;
    }
    // 排板条件未满足不下单
    if (!should_enter(_latest_board_amount, _latest_board_lots))
    {
        return;
    }

    // 限价单按涨停价；市价单使用买一价作为传参价格
    const double order_price = (_order_flag == eOrderFlag::Limit ? tick.upper_limit_price : tick.bid_price[0]);
    // 发起买入开仓排板
    const orderref_t order_ref = buy_open(_order_flag, _contract, order_price, _quantity);
    if (order_ref != null_orderref)
    {
        // 记录活动订单，并锁定“首单已使用”
        _active_orderref = order_ref;
        _has_placed_once = true;
    }
}

void board_queue::on_order(const Order &order)
{
    // 只给当前活动单绑定撤单条件
    if (order.order_ref != _active_orderref)
    {
        return;
    }
    set_cancel_condition(order.order_ref, [this](orderref_t order_ref)-> bool
    {
        if (order_ref != _active_orderref)
        {
            return false;
        }
        // 优先使用最新 tick 时间；若暂无 tick 则使用本机时间
        const std::string t = _latest_tick_time.empty() ? now_hhmmss() : _latest_tick_time;
        // 超过有效时间立即撤单
        if (!in_active_window(t))
        {
            return true;
        }
        // 触发任一撤单条件即撤单
        if (should_exit(_latest_board_amount, _latest_board_lots))
        {
            return true;
        }
        return false;
    });
}

void board_queue::clear_active_order(orderref_t order_ref)
{
    // 活动单生命周期结束后清空引用
    if (_active_orderref == order_ref)
    {
        _active_orderref = null_orderref;
    }
}

void board_queue::on_trade(const Order &order)
{
    // 成交通知：清理活动单引用
    clear_active_order(order.order_ref);
}

void board_queue::on_cancel(const Order &order)
{
    // 撤单通知：清理活动单引用
    clear_active_order(order.order_ref);
}

void board_queue::on_error(const Order &order)
{
    // 报错通知：清理活动单引用
    clear_active_order(order.order_ref);
}

void board_queue::on_update()
{
    // 周期回调做超时兜底，避免无新 tick 时挂单长时间留存
    if (!_inited)
    {
        return;
    }
    if (_active_orderref == null_orderref)
    {
        return;
    }
    const std::string t = _latest_tick_time.empty() ? now_hhmmss() : _latest_tick_time;
    if (!in_active_window(t))
    {
        // 超时主动撤活动单
        cancel_order(_active_orderref);
    }
}
