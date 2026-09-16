#include "strategy.h"
#include "datafeed.h"
#include <cmath>
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

/*
(targetVol / σ) × equity / price
dailyVol = standard deviation of dailyReturns across lookback period
Var = 1/(N-1) Σ(rᵢ − r̄)²  = (Σ rᵢ² − (Σ rᵢ)²/N)/(N-1)
thus need to keep track of Σ rᵢ and Σ rᵢ²

σ = √ Var

keeps deque<double returns>
*/

class VolatilityTargetSizer : public Sizer {
public:
  explicit VolatilityTargetSizer(int minLookback, double targetVol)
      : Sizer(minLookback), targetVol_(targetVol) {}

  double generate(int signalOutput, State &state,
                  const std::vector<Bar> &history) override {
    int N = getMinLookback();

    if (history.size() > 1) {
      double currReturn =
          history.back().close / history[history.size() - 2].close - 1;
      returnWindow.addVal(currReturn);
    }
    if (returnWindow.size() < N)
      return 0;

    double price = history.back().close;
    double stdDev = returnWindow.getStdDev();
    double equity = state.getEquity(price);
    return (targetVol_ / stdDev) * equity / price;
  }

private:
  double targetVol_{};
  RollingWindow<double> returnWindow{getMinLookback()};
};

} // namespace Sizers

namespace RiskManagers {
/*
notional cap is hard limit on the total notional exposure a strategy is allowed
to hold at any moment

Notional = shares x price

Hard limit = absolute ceiling, never exceed this amount
Scaling = how the cap behaves below ceiling 

*/

class NotionalCapRiskManager : public RiskManager {};

} // namespace RiskManagers
