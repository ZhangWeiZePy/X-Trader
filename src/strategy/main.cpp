#include "frame.h"
#include "strategy_loader.h"
#include <cstdio>

void start_running(const char *filename)
{
    frame run(filename);
    auto strategys = create_strategies_from_ini(filename, run);
    if (strategys.empty())
    {
        return;
    }
    run.run_until_close(strategys);
}


int main(int argc, char **argv)
{
    if (argc < 2)
    {
        printf("usage: femtotrader <ini_path>\n");
        return 1;
    }
    start_running(argv[1]);
    return 0;
}
