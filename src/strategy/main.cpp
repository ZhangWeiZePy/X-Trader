#include "frame.h"
#include "strategy_loader.h"
#include <spdlog/spdlog.h>

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
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%!] %v");
#ifndef NDEBUG
    spdlog::set_level(spdlog::level::debug);
#else
    spdlog::set_level(spdlog::level::info);
#endif
    if (argc < 2)
    {
        spdlog::error("usage: femtotrader <ini_path>");
        return 1;
    }
    start_running(argv[1]);
    return 0;
}
