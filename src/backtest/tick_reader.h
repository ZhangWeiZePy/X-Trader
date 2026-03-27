#pragma once

#include <fstream>
#include <string>
#include "data_struct.h"

class tick_reader
{
public:
	tick_reader() {}
	~tick_reader();

	bool open(const std::string& path);
	bool read_next(MarketData& tick);
	void close();

private:
	bool parse_line(const std::string& line, MarketData& tick) const;

private:
	std::ifstream _ifs;
};
