#include "frame.h"
#include "strategy_loader.h"
#include <cstdio>

void start_running(const char* filename)
{
	frame run(filename);
	auto strat = create_strategy_from_ini(filename, run);
	if (!strat) { return; }
	std::vector<std::shared_ptr<strategy>> strategys;
	strategys.emplace_back(strat);
	run.run_until_close(strategys);
}


int main(int argc, char** argv)
{
	if (argc < 2)
	{
		printf("usage: femtotrader <ini_path>\n");
		return 1;
	}
	start_running(argv[1]);
	return 0;
}
