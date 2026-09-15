#include "strategy.h"
#include "datafeed.h"

int StrategyImproved::getMove(State &state, const std::vector<Bar> &history) {
  auto signalOutput = signal_.generate(state, history);
  auto sizerOutput = sizer_.generate(signalOutput, state, history);
  return riskManager_.generate(sizerOutput, state, history);
}


namespace Signals {
class BuyHoldSignal : public Signal {
  int generate(State &state, const std::vector<Bar> &history) override {
    if (history.size() == 0)
      return 1;
    if (state.getTotalBars() == history.size() + 1) {
      return -1;
    }
    return 0;
  }
};
} // namespace Signals

namespace Sizers {
class FixedFractionalSizer : public Sizer {
public:
  explicit FixedFractionalSizer(double f) : f_(f) {}
  double generate(int signalOutput, State &state,
                  const std::vector<Bar> &history) override {
    double price = history.back().open;
    return signalOutput * (f_ * state.getEquity(price)) / price;
  }

private:
  double f_{};
};


// keeps deque<double closePrices> 
class VolatilityTargetSizer : public Sizer {
public:
  explicit VolatilityTargetSizer(int minLookback, double targetVol): Sizer(minLookback), targetVol_(targetVol) {}
  double generate(int signalOutput, State &state, const std::vector<Bar> &history) override {
    if (minLookback)
    
  }


private:
  double targetVol_{};

};

} // namespace Sizers

namespace RiskManagers {}
