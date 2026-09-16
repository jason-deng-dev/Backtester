#include "strategy.h"
#include "datafeed.h"
#include <cmath>
#include <cstdlib>
#include <numeric>

int StrategyImproved::getMove(const State &state, const std::vector<Bar> &history) {
  auto signalOutput = signal_.generate(state, history);
  if (signalOutput == 0) return 0;
  auto sizerOutput = sizer_.generate(signalOutput, state, history);
  if (sizerOutput == 0) return 0;
  return riskManager_.generate(sizerOutput, state, history);
}


class Signals::BuyHoldSignal : public Signal {
  int generate(const State &state, const std::vector<Bar> &history) override {
    if (history.size() == 0)
      return 1;
    if (state.getTotalBars() == history.size() + 1) {
      return -1;
    }
    return 0;
  }
};



class Sizers::FixedFractionalSizer : public Sizer {
public:
  explicit FixedFractionalSizer(double f) : f_(f) {}
  double generate(int signalOutput, const State &state,
                  const std::vector<Bar> &history) override {
    if (history.empty()) return 0;
    double price = history.back().close;
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

class Sizers::VolatilityTargetSizer : public Sizer {
public:
  explicit VolatilityTargetSizer(int minLookback, double targetVol)
      : Sizer(minLookback), targetVol_(targetVol) {}

  double generate(int signalOutput, const State &state,
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
    std::optional<double> stdDev = returnWindow.getStdDev();
    // not enough data points
    if (!stdDev) return 0;
    double equity = state.getEquity(price);
    return (targetVol_ / *stdDev) * equity / price;
  }

private:
  double targetVol_{};
  RollingWindow<double> returnWindow{getMinLookback()};
};



/*
notional cap is hard limit on the total notional exposure a strategy is allowed
to hold at any moment

Notional = shares x price

ceiling: fraction of equity
*/

class RiskManagers::NotionalCapRiskManager : public RiskManager {
public:
  NotionalCapRiskManager(double ceilRatio) : ceilRatio_(ceilRatio) {}
  double generate(double sizerOutput, const State &state, const std::vector<Bar> &history) override {
    if (history.size() == 0 || sizerOutput == 0) return 0;
    double price = history.back().close;
    double capNotional = state.getEquity(price) * ceilRatio_;
    
    double proposedNotional = (state.getNetQty() + sizerOutput)*price;

    if (std::abs(capNotional) >= std::abs(proposedNotional) || state.getNetQty() > state.getNetQty()+sizerOutput) {
      return sizerOutput;
    }
    double maxPos = capNotional / price;
    double allowedAmount = std::abs(maxPos) - std::abs(state.getNetQty());

    return sizerOutput/std::abs(sizerOutput) * allowedAmount;
  }

private:
  double ceilRatio_;
};


class RiskManagers::StopLossRiskManager : public RiskManager {
public:
  StopLossRiskManager(){}


};

