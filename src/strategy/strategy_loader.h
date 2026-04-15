#pragma once

#include <memory>
#include <string>
#include <vector>
#include "strategy.h"

class frame;

std::vector<std::shared_ptr<strategy> > create_strategies_from_ini(const std::string &ini_path, frame &run);
