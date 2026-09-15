#pragma once

#include "datafeed.h"
class Strategy;
class State;
 

class Backtest {
public:
  std::vector<Bar> history_;
  void run(DataFeed &feed, Strategy& strategy, State& state);
};
