#include "strategy_loader.h"

#include <cstdio>
#include <filesystem>
#include <limits>
#include <map>
#include <set>
#include <sstream>

#include "frame.h"
#include "INIReader.h"
#include "board_queue.h"

namespace
{
    std::string trim_copy(const std::string &value)
    {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos)
        {
            return "";
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    void load_board_queue_config(INIReader &reader, const std::string &config_section,
                                 std::map<std::string, std::string> &config)
    {
        config["contract"] = reader.Get(config_section, "contract", "");
        config["order_type"] = reader.Get(config_section, "order_type", "");
        config["quantity"] = reader.Get(config_section, "quantity", "");
        config["active_start_time"] = reader.Get(config_section, "active_start_time", "");
        config["active_end_time"] = reader.Get(config_section, "active_end_time", "");
        config["enable_queue_amount_enter"] = reader.Get(config_section, "enable_queue_amount_enter", "");
        config["queue_amount_enter"] = reader.Get(config_section, "queue_amount_enter", "");
        config["enable_queue_lots_enter"] = reader.Get(config_section, "enable_queue_lots_enter", "");
        config["queue_lots_enter"] = reader.Get(config_section, "queue_lots_enter", "");
        config["enable_queue_amount_exit"] = reader.Get(config_section, "enable_queue_amount_exit", "");
        config["queue_amount_exit"] = reader.Get(config_section, "queue_amount_exit", "");
        config["enable_queue_lots_exit"] = reader.Get(config_section, "enable_queue_lots_exit", "");
        config["queue_lots_exit"] = reader.Get(config_section, "queue_lots_exit", "");
        config["allow_reenter_after_cancel"] = reader.Get(config_section, "allow_reenter_after_cancel", "false");
        config["max_reenter_times"] = reader.Get(config_section, "max_reenter_times", "0");
    }
}

std::vector<std::shared_ptr<strategy> > create_strategies_from_ini(const std::string &ini_path, frame &run)
{
    std::vector<std::shared_ptr<strategy> > strategies;
    if (!std::filesystem::exists(ini_path))
    {
        printf("strategy config file not found: %s\n", ini_path.c_str());
        return {};
    }

    INIReader reader(ini_path);
    if (reader.ParseError() < 0)
    {
        printf("can't load %s\n", ini_path.c_str());
        return {};
    }

    const std::string strategy_list = reader.Get("strategy", "list", "");
    if (strategy_list.empty())
    {
        printf("strategy.list is required\n");
        return {};
    }

    std::set<std::string> alias_set;
    std::set<long> id_set;
    std::stringstream ss(strategy_list);
    std::string alias;
    while (std::getline(ss, alias, ','))
    {
        const std::string alias_trimmed = trim_copy(alias);
        if (alias_trimmed.empty())
        {
            printf("strategy.list contains empty alias\n");
            return {};
        }
        if (!alias_set.insert(alias_trimmed).second)
        {
            printf("duplicate strategy alias in strategy.list: %s\n", alias_trimmed.c_str());
            return {};
        }

        const std::string common_section = "strategy." + alias_trimmed;
        const std::string strategy_name = reader.Get(common_section, "name", "");
        const long strategy_id = reader.GetInteger(common_section, "id", -1);

        if (strategy_name.empty())
        {
            printf("%s.name is required\n", common_section.c_str());
            return {};
        }
        if (strategy_id <= 0)
        {
            printf("%s.id must be positive\n", common_section.c_str());
            return {};
        }
        if (strategy_id > std::numeric_limits<stratid_t>::max())
        {
            printf("%s.id exceeds max allowed value: %lld\n", common_section.c_str(),
                   static_cast<long long>(strategy_id));
            return {};
        }
        if (!id_set.insert(strategy_id).second)
        {
            printf("duplicate strategy id: %lld\n", static_cast<long long>(strategy_id));
            return {};
        }

        const std::string config_section = common_section + "." + strategy_name;
        std::map<std::string, std::string> config;
        std::shared_ptr<strategy> strat;

        if (strategy_name == "board_queue")
        {
            strat = std::make_shared<board_queue>(static_cast<stratid_t>(strategy_id), run);
            load_board_queue_config(reader, config_section, config);
        } else
        {
            printf("unsupported %s.name: %s\n", common_section.c_str(), strategy_name.c_str());
            return {};
        }

        if (!strat->set_config(config))
        {
            printf("invalid strategy config in section: %s\n", config_section.c_str());
            return {};
        }
        strategies.emplace_back(strat);
    }

    if (strategies.empty())
    {
        printf("strategy.list is empty\n");
        return {};
    }

    return strategies;
}
