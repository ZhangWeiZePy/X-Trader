#include "frame.h"
#include "INIReader.h"
#include "percent_change_strategy.h"
#include <cmath>

struct strategy_config
{
	std::string contract = "600850";
	double buy_trigger_pct = 2.0;
	double sell_trigger_pct = -2.0;
	uint32_t position_limit = 1000;
	int once_vol = 100;
};

strategy_config load_strategy_config(const char* filename)
{
	strategy_config cfg;
	INIReader reader(filename);
	if (reader.ParseError() < 0) { return cfg; }

	cfg.contract = reader.Get("strategy", "contract", cfg.contract);
	cfg.buy_trigger_pct = reader.GetReal("strategy", "buy_trigger_pct", cfg.buy_trigger_pct);
	cfg.sell_trigger_pct = reader.GetReal("strategy", "sell_trigger_pct", cfg.sell_trigger_pct);
	cfg.position_limit = static_cast<uint32_t>(reader.GetInteger("strategy", "position_limit", cfg.position_limit));
	cfg.once_vol = static_cast<int>(reader.GetInteger("strategy", "once_vol", cfg.once_vol));

	if (cfg.sell_trigger_pct > 0) { cfg.sell_trigger_pct = -std::fabs(cfg.sell_trigger_pct); }
	if (cfg.position_limit == 0) { cfg.position_limit = 100; }
	if (cfg.once_vol <= 0) { cfg.once_vol = 100; }
	if (cfg.once_vol > static_cast<int>(cfg.position_limit)) { cfg.once_vol = static_cast<int>(cfg.position_limit); }
	return cfg;
}

void start_running(const char* filename)
{
	frame run(filename);
	strategy_config cfg = load_strategy_config(filename);
	std::vector<std::shared_ptr<strategy>> strategys;
	strategys.emplace_back(std::make_shared<percent_change_strategy>(
		1,
		run,
		cfg.contract,
		cfg.buy_trigger_pct,
		cfg.sell_trigger_pct,
		cfg.position_limit,
		cfg.once_vol));
	run.run_until_close(strategys);
}


int main()
{
	start_running("./ini/simnow/3_117509.ini");
	return 0;
}
