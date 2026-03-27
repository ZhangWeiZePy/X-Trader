#pragma once

#include <memory>
#include <string>
#include "strategy.h"

class frame;

std::shared_ptr<strategy> create_strategy_from_ini(const std::string& ini_path, frame& run);
