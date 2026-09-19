#pragma once
#include "datafeed.h"
#include "state.h"
#include "strategy.h"

class Sizer {
public:
  virtual ~Sizer() = default;
  virtual double generate(int signalOutput, const State &state,
                          const std::vector<Bar> &history) = 0;
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
thus need to keep track ofq
q Σ rᵢ and Σ rᵢ²

σ = √ Var

keeps deque<double returns>
*/

class VolatilityTargetSizer : public Sizer {
public:
  explicit VolatilityTargetSizer(int minLookback, double targetVol)
      : targetVol_(targetVol), returnWindow(minLookback) {}

  double generate(int signalOutput, const State &state,
                  const std::vector<Bar> &history) override {

    if (history.size() > 1) {
      double currReturn =
          history.back().close / history[history.size() - 2].close - 1;
      returnWindow.addVal(currReturn);
    }
    else {
      return 0;
    }

    double price = history.back().close;
    std::optional<double> stdDev = returnWindow.getStdDev();
    // not enough data points
    if (!stdDev)
      return 0;
    double equity = state.getEquity(price);
    return signalOutput * (targetVol_ / *stdDev) * equity / price;
  }

private:
  double targetVol_{};
  RollingWindow<double> returnWindow;
};
