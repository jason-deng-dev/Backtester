#include "strategy.h"
#include "datafeed.h"

int StrategyImproved::getMove(State &state, const std::vector<Bar> &history) {
  auto signalOutput = signal_.generate(state, history);
  auto sizerOutput = sizer_.generate(signalOutput, state, history);
  return riskManager_.generate(sizerOutput, state, history);
}
