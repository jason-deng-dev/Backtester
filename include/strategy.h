#pragma once

#include "types.h"
#include <vector>

class Signal;
class Sizer;
class RiskManager;
class State;

class Strategy {
  Signal &signal_;
  Sizer &sizer_;
  RiskManager &riskManager_;

public:
  Strategy(Signal &signal, Sizer &sizer, RiskManager &riskManager)
      : signal_(signal), sizer_(sizer), riskManager_(riskManager) {}
  int getMove(const State &state, const std::vector<Bar> &history);
};
