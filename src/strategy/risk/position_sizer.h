#pragma once

class position_sizer
{
public:
	struct config
	{
		double max_capital_per_symbol = 300000;
		int lot_size = 100;
		int max_order_lots = 10;
	};

public:
	position_sizer() {}
	explicit position_sizer(const config& cfg) : _cfg(cfg) {}

	void set_config(const config& cfg) { _cfg = cfg; }

	int calc_order_volume(double cash_available, double price, double symbol_used_capital) const;

private:
	config _cfg;
};
