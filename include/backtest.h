#pragma once

class DataFeed;
class Strategy;
class State;
 

class Backtest {
public:
  void run(DataFeed &feed, Strategy& strategy, State& state);
};
