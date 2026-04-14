#include "interface.h"
#include "market/xtp_market.h"
#include "trader/xtp_trader.h"
#include "market/tora_market.h"
#include "trader/tora_trader.h"

market_api *create_market(std::map<std::string, std::string> &config, std::set<std::string> contracts)
{
    if (config["counter"] == "xtp")
    {
        return new xtp_market(config, contracts);
    }
    if (config["counter"] == "tora")
    {
        return new tora_market(config, contracts);
    }
    return nullptr;
}

void destory_market(market_api *&api)
{
    if (nullptr != api)
    {
        delete api;
        api = nullptr;
    }
}

trader_api *create_trader(std::map<std::string, std::string> &config, std::set<std::string> contracts)
{
    if (config["counter"] == "xtp")
    {
        return new xtp_trader(config, contracts);
    }
    if (config["counter"] == "tora")
    {
        return new tora_trader(config, contracts);
    }
    return nullptr;

}

void destory_trader(trader_api *&api)
{
    if (nullptr != api)
    {
        delete api;
        api = nullptr;
    }
}
