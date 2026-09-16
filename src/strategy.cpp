#include "strategy.h"
#include "datafeed.h"
#include <numeric>

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

// (targetVol / σ) × equity / price
// dailyVol = standard deviation of dailyReturns across lookback period
// need mean of returns across period
// compute each squared deviation (ri-rMean)^2
// each new window, update rMean, and ∑(ri-rM)
// keeps deque<double returns>
class VolatilityTargetSizer : public Sizer {
public:
  explicit VolatilityTargetSizer(int minLookback, double targetVol)
      : Sizer(minLookback), targetVol_(targetVol) {}
  double generate(int signalOutput, State &state,
                  const std::vector<Bar> &history) override {
    if (getMinLookback() + 1 > history.size()) {
      return 0;
    } else if (getMinLookback() + 1 == history.size()) {
      for (int i = 0; i < history.size() - 1; ++i) {
        double pCurr = history[i].close;
        double pNext = history[i + 1].close;
        double currReturn = pNext / pCurr - 1;
        returnTotal += currReturn;
        returns.push_back(currReturn);
      }
    } else {
      double pPrev = history[history.size() - 2].close;
      double pCurr = history.back().close;
      double newReturn = pCurr / pPrev - 1;
      returns.push_back(newReturn);
      returnTotal += -returns.front() + newReturn;
      returns.pop_front();
    }
  }

private:
  double targetVol_{};
  double returnTotal;
  std::deque<double> returns;
};

} // namespace Sizers

namespace RiskManagers {}
