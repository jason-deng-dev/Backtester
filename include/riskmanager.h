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
  NotionalCapRiskManager(double ceilRatio)
      : ceilRatio_(ceilRatio) {}
  double generate(double sizerOutput, const State &state,
                  const std::vector<Bar> &history) override {
    if (history.size() == 0 || sizerOutput == 0)
      return 0;

    double price = history.back().close;

    // allowed range for notional = [-cap, cap]
    double capNotional = state.getEquity(price) * ceilRatio_;

    double netQty = state.getNetQty();
    double newQty = netQty + sizerOutput;

    double proposedNotional = newQty * price;

    // covers undercap, reduction, oncap => rest has to be overcap
    if (std::abs(capNotional) >= std::abs(proposedNotional)) {
      return sizerOutput;
    }
    
    bool stayLong = netQty < netQty + sizerOutput && netQty >= 0;
    bool stayShort = netQty > netQty + sizerOutput && netQty <= 0;

    

    if (stayLong || stayShort) {
      double maxQty = proposedNotional/price;
      return maxQty - netQty;
    }
    // reversal overCap: closing the old side is risk-reducing and always
    // allowed, then establish up to the cap on the new side
    else {
      double capQty = capNotional / price;
      double newSideSign = sizerOutput > 0 ? 1.0 : -1.0;
      return newSideSign * capQty - netQty;
    }
  }

private:
  double ceilRatio_;
};

class StopLossRiskManager : public RiskManager {
public:
  StopLossRiskManager() {}
};
