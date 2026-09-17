#pragma once

#include "datafeed.h"
#include "state.h"
#include "strategy.h"
#include <algorithm>
#include <cstdlib>
#include <deque>
#include <optional>
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

class NotionalCapRiskManager : public RiskManager {
  /*
  notional cap is hard limit on the total notional exposure a strategy is
  allowed to hold at any moment

  Notional = shares x price

  ceiling: fraction of equity
  */
public:
  explicit NotionalCapRiskManager(double ceilRatio) : ceilRatio_(ceilRatio) {}
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
    if (std::abs(capNotional) >= std::abs(proposedNotional) ||
        std::abs(newQty) <= std::abs(netQty)) {
      return sizerOutput;
    }

    bool stayLong = netQty < netQty + sizerOutput && netQty >= 0;
    bool stayShort = netQty > netQty + sizerOutput && netQty <= 0;

    if (stayLong || stayShort) {
      int dir = stayLong ? +1 : -1;
      double maxQty = dir * capNotional / price;

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

class TrueRanges {
public:
  explicit TrueRanges(int ATR_range) : ATR_range_(ATR_range) {};

  std::optional<double> getATR() {
    if (count < ATR_range_)
      return std::nullopt;
    else
      return ATR;
  }

  void addTR(double TR) {
    count++;
    total += TR;
    if (count == ATR_range_) {
      ATR = total / double(count);
    } else {
      ATR = (ATR * (count - 1) + TR) / count;
    }
  }

private:
  double ATR{};
  int total = 0;
  int count{};
  int ATR_range_{};
};

class BracketRiskManager : public RiskManager {

public:
  explicit BracketRiskManager(int ATR_range, double stopLossMulti,
                              double takeProfitMulti)
      : RiskManager(ATR_range), trs(ATR_range), stopLossMulti_(stopLossMulti),
        takeProfitMulti_(takeProfitMulti) {}

  double generate(double sizerOutput, const State &state,
                  const std::vector<Bar> &history) override {
    if (history.size() < 2)
      return sizerOutput;
    auto &currBar = history.back();
    auto &prevBar = history[history.size() - 2];

    double TR = std::max({currBar.high - currBar.low,
                          std::abs(currBar.high - prevBar.close),
                          std::abs(currBar.low - prevBar.close)});
    trs.addTR(TR);
    auto ATR = trs.getATR();
    if (!ATR) return sizerOutput;


  }

private:
  TrueRanges trs;
  double stopLossMulti_;
  double takeProfitMulti_;
};
