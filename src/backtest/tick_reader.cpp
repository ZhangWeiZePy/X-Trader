#include "tick_reader.h"

#include <cstring>
#include <sstream>
#include <vector>

tick_reader::~tick_reader()
{
	close();
}

bool tick_reader::open(const std::string& path)
{
	close();
	_ifs.open(path);
	return _ifs.good();
}

bool tick_reader::read_next(MarketData& tick)
{
	if (!_ifs.good()) { return false; }
	std::string line;
	while (std::getline(_ifs, line))
	{
		if (line.empty()) { continue; }
		if (parse_line(line, tick)) { return true; }
	}
	return false;
}

void tick_reader::close()
{
	if (_ifs.is_open()) { _ifs.close(); }
}

bool tick_reader::parse_line(const std::string& line, MarketData& tick) const
{
	std::vector<std::string> cols;
	std::stringstream ss(line);
	std::string c;
	while (std::getline(ss, c, ','))
	{
		cols.push_back(c);
	}
	if (cols.size() < 10) { return false; }
	std::memset(&tick, 0, sizeof(MarketData));
	std::strncpy(tick.instrument_id, cols[1].c_str(), sizeof(tick.instrument_id) - 1);
	std::strncpy(tick.update_time, cols[2].c_str(), sizeof(tick.update_time) - 1);
	tick.update_millisec = std::stoi(cols[3]);
	tick.last_price = std::stod(cols[4]);
	tick.upper_limit_price = std::stod(cols[5]);
	tick.bid_price[0] = std::stod(cols[6]);
	tick.bid_volume[0] = std::stoi(cols[7]);
	tick.ask_price[0] = std::stod(cols[8]);
	tick.ask_volume[0] = std::stoi(cols[9]);
	return true;
}
