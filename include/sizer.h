#pragma once
#include "state.h"
#include "datafeed.h"
#include "strategy.h"

class Sizer {
public:
  Sizer(int minLookback = 0) : minLookback_(minLookback) {}
  virtual ~Sizer() = default;
  virtual double generate(int signalOutput, const State &state,
                          const std::vector<Bar> &history) = 0;
  int getMinLookback() const { return minLookback_; }

private:
  int minLookback_{}; // called by generate to verify we have enough history
};


class FixedFractionalSizer : public Sizer {
public:
  explicit FixedFractionalSizer(double f) : f_(f) {}
  double generate(int signalOutput, const State &state,
                  const std::vector<Bar> &history) override {
    if (history.empty())
      return 0;
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

class VolatilityTargetSizer : public Sizer {
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
    if (!stdDev)
      return 0;
    double equity = state.getEquity(price);
    return (targetVol_ / *stdDev) * equity / price;
  }

private:
  double targetVol_{};
  RollingWindow<double> returnWindow{getMinLookback()};
};

