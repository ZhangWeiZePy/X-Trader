#include "strategy_loader.h"

#include <cstdio>
#include <filesystem>
#include <map>

#include "frame.h"
#include "INIReader.h"
#include "limitup_strategy.h"
#include "market_making.h"

std::shared_ptr<strategy> create_strategy_from_ini(const std::string& ini_path, frame& run)
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
	if (strategy_name == "market_making")
	{
		auto strat = std::make_shared<market_making>(static_cast<stratid_t>(strategy_id), run);
		std::map<std::string, std::string> config;
		config["contract"] = reader.Get("strategy.market_making", "contract", "");
		config["price_delta"] = reader.Get("strategy.market_making", "price_delta", "");
		config["position_limit"] = reader.Get("strategy.market_making", "position_limit", "");
		config["once_vol"] = reader.Get("strategy.market_making", "once_vol", "");
		if (!strat->set_config(config))
		{
			printf("invalid market_making strategy config\n");
			return nullptr;
		}
		return strat;
	}
	if (strategy_name == "limitup")
	{
		auto strat = std::make_shared<limitup_strategy>(static_cast<stratid_t>(strategy_id), run);
		std::map<std::string, std::string> config;
		config["contract"] = reader.Get("strategy.limitup", "contract", "");
		config["contracts"] = reader.Get("strategy.limitup", "contracts", "");
		config["mode_market"] = reader.Get("strategy.limitup", "mode_market", "mainboard");
		config["mode_tempo"] = reader.Get("strategy.limitup", "mode_tempo", "first_and_reseal");
		config["mode_exec"] = reader.Get("strategy.limitup", "mode_exec", "queue_first");
		config["tick_epsilon"] = reader.Get("strategy.limitup", "tick_epsilon", "0.0001");
		config["min_seal_lot"] = reader.Get("strategy.limitup", "min_seal_lot", "100000");
		config["min_seal_amount"] = reader.Get("strategy.limitup", "min_seal_amount", "0");
		config["reseal_window_ms"] = reader.Get("strategy.limitup", "reseal_window_ms", "15000");
		config["max_pullback_bp"] = reader.Get("strategy.limitup", "max_pullback_bp", "80");
		config["signal_cooldown_sec"] = reader.Get("strategy.limitup", "signal_cooldown_sec", "60");
		config["order_lot"] = reader.Get("strategy.limitup", "order_lot", "100");
		config["max_order_lots"] = reader.Get("strategy.limitup", "max_order_lots", "10");
		config["queue_timeout_ms"] = reader.Get("strategy.limitup", "queue_timeout_ms", "1200");
		if (!strat->set_config(config))
		{
			printf("invalid limitup strategy config\n");
			return nullptr;
		}
		return strat;
	}
	printf("unsupported strategy.name: %s\n", strategy_name.c_str());
	return nullptr;
}
