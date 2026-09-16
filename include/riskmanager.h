#pragma once

#include "state.h"
#include "datafeed.h"
#include <cstdlib>
#include <vector>


class RiskManager {
public:
  RiskManager(int minLookback = 0) : minLookback_(minLookback) {}
  virtual ~RiskManager() = default;
  virtual double generate(double sizerOutput, const State &state,
                          const std::vector<Bar> &history) = 0;
  int getMinLookback() const { return minLookback_; }

private:
  int minLookback_{}; // called by generate to verify we have enough history
};

/*
notional cap is hard limit on the total notional exposure a strategy is allowed
to hold at any moment

Notional = shares x price

ceiling: fraction of equity
*/

class NotionalCapRiskManager : public RiskManager {
public:
  enum class Policy { Clamp, Correct };
  NotionalCapRiskManager(double ceilRatio, Policy policy)
      : ceilRatio_(ceilRatio), policy_(policy) {}
  double generate(double sizerOutput, const State &state,
                  const std::vector<Bar> &history) override {
    if (history.size() == 0 || sizerOutput == 0)
      return 0;
    double price = history.back().close;
    double capNotional = state.getEquity(price) * ceilRatio_;

    double proposedNotional = (state.getNetQty() + sizerOutput) * price;

    if (std::abs(capNotional) >= std::abs(proposedNotional) ||
        state.getNetQty() > state.getNetQty() + sizerOutput) {
      return sizerOutput;
    }
    double maxPos = capNotional / price;
    double allowedAmount = std::abs(maxPos) - std::abs(state.getNetQty());

    return sizerOutput / std::abs(sizerOutput) * allowedAmount;
  }

private:
  double ceilRatio_;
  Policy policy_;
};

class StopLossRiskManager : public RiskManager {
public:
  StopLossRiskManager() {}
};
