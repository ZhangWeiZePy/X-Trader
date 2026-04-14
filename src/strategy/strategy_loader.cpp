#include "strategy_loader.h"

#include <cstdio>
#include <filesystem>
#include <map>

#include "frame.h"
#include "INIReader.h"
#include "board_queue.h"

std::shared_ptr<strategy> create_strategy_from_ini(const std::string &ini_path, frame &run)
{
    if (!std::filesystem::exists(ini_path))
    {
        printf("strategy config file not found: %s\n", ini_path.c_str());
        return nullptr;
    }
    INIReader reader(ini_path);
    if (reader.ParseError() < 0)
    {
        printf("can't load %s\n", ini_path.c_str());
        return nullptr;
    }
    const std::string strategy_name = reader.Get("strategy", "name", "");
    const long strategy_id = reader.GetInteger("strategy", "id", 1);
    if (strategy_name.empty())
    {
        printf("strategy.name is required\n");
        return nullptr;
    }
    if (strategy_id <= 0)
    {
        printf("strategy.id must be positive\n");
        return nullptr;
    }
    if (strategy_name == "board_queue")
    {
        auto strat = std::make_shared<board_queue>(static_cast<stratid_t>(strategy_id), run);
        std::map<std::string, std::string> config;
        config["contract"] = reader.Get("strategy.board_queue", "contract", "");
        config["order_type"] = reader.Get("strategy.board_queue", "order_type", "");
        config["quantity"] = reader.Get("strategy.board_queue", "quantity", "");
        config["active_start_time"] = reader.Get("strategy.board_queue", "active_start_time", "");
        config["active_end_time"] = reader.Get("strategy.board_queue", "active_end_time", "");
        config["enable_queue_amount_enter"] = reader.Get("strategy.board_queue", "enable_queue_amount_enter", "");
        config["queue_amount_enter"] = reader.Get("strategy.board_queue", "queue_amount_enter", "");
        config["enable_queue_lots_enter"] = reader.Get("strategy.board_queue", "enable_queue_lots_enter", "");
        config["queue_lots_enter"] = reader.Get("strategy.board_queue", "queue_lots_enter", "");
        config["enable_queue_amount_exit"] = reader.Get("strategy.board_queue", "enable_queue_amount_exit", "");
        config["queue_amount_exit"] = reader.Get("strategy.board_queue", "queue_amount_exit", "");
        config["enable_queue_lots_exit"] = reader.Get("strategy.board_queue", "enable_queue_lots_exit", "");
        config["queue_lots_exit"] = reader.Get("strategy.board_queue", "queue_lots_exit", "");
        config["allow_reenter_after_cancel"] = reader.Get("strategy.board_queue", "allow_reenter_after_cancel", "false");
        config["max_reenter_times"] = reader.Get("strategy.board_queue", "max_reenter_times", "0");
        if (!strat->set_config(config))
        {
            printf("invalid board_queue strategy config\n");
            return nullptr;
        }
        return strat;
    }
    printf("unsupported strategy.name: %s\n", strategy_name.c_str());
    return nullptr;
}
