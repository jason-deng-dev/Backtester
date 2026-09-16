#include "strategy.h"
#include "signal.h"
#include "sizer.h"
#include "riskmanager.h"

int Strategy::getMove(const State &state,
                              const std::vector<Bar> &history) {
  auto signalOutput = signal_.generate(state, history);
  if (signalOutput == 0)
    return 0;
  auto sizerOutput = sizer_.generate(signalOutput, state, history);
  if (sizerOutput == 0)
    return 0;
  return riskManager_.generate(sizerOutput, state, history);
}

