#include "frame.h"
#include "market_making.h"

void start_running(const char* filename)
{
    frame run(filename);
    std::vector<std::shared_ptr<strategy>> strategys;
    strategys.emplace_back(std::make_shared<market_making>(1, run, "rb2409", 2, 10, 1));
	run.run_until_close(strategys);
}


int main()
{
	start_running("./ini/simnow/3_117509.ini");
	return 0;
}
