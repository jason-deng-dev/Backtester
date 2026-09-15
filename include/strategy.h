#ifndef STRATEGY_H
#define STRATEGY_H

#include "state.h"
#include <algorithm>
#include <cstddef>
#include <queue>

class Strategy {
public:
  Strategy(int maxHistorySize_ = 0) : maxHistorySize{maxHistorySize_} {}
  // return positive to buy, negative to sell
  virtual int getMove(State &, double openPrice, double closePrice) = 0;

protected:
  void addPrice(double price) {
    if (maxHistorySize == 0)
      return;
    closePriceHistory.push(price);
    if (closePriceHistory.size() > static_cast<std::size_t>(maxHistorySize))
      closePriceHistory.pop();
  };

  std::queue<double> closePriceHistory;

  virtual int exitCondition(State &state,
                            double openPrice) = 0; // 0 if don't exit, >0 to buy
                                                   // (short), <0 to sell (long)
  virtual int
  entryCondition(State &state,
                 double openPrice) = 0; // 0 if don't enter, <0 to sell
                                        // (short), >0 to buy (long)

  int maxHistorySize;
};

/*
if close price is lower than open price, we expect reversion, take long position
if close price is higher than open price, we expect reversion, take short
position

naive implementation
- only buy and sell 10 shares at a time
- after buying, sell right after in next bar
- just need last close price to be higher than current open price to decide to
buy (tracks only last priceHistory)
*/

class naive_reversion_strategy : public Strategy {
public:
  naive_reversion_strategy() : Strategy(1) {}

  int getMove(State &state, double openPrice, double closePrice) override {
    if (closePriceHistory.empty()) {
      addPrice(closePrice);
      return 0;
    }

    int move =
        exitCondition(state, openPrice) + entryCondition(state, openPrice);
    addPrice(closePrice);

    return move;
  }

protected:
  int exitCondition(State &state, double openPrice) override {
    return -state.getNetQty();
  }

  int entryCondition(State &state, double openPrice) override {
    double qty =
        (state.getCash() + openPrice * state.getNetQty()) * 0.05 / openPrice;

    if (closePriceHistory.back() < openPrice) {
      return qty;
    } else if (closePriceHistory.back() > openPrice) {
      return -qty;
    } else
      return 0;
  }
};

class Signal {
public:
  virtual ~Signal() = default;
  virtual int maxLookback() const = 0;
};

class Sizer {
public:
  virtual ~Sizer() = default;
  virtual int maxLookback() const = 0;
};

class RiskManager {
public:
  virtual ~RiskManager() = default;
  virtual int maxLookback() const = 0;
};

class buyHoldSignal : public Signal {
public:
  int maxLookback() const override { return 0; }
};

class StrategyImproved {
  int maxLookback_{};
  Signal& signal_;
  Sizer& sizer_;
  RiskManager& riskManager_;

  StrategyImproved(Signal &signal, Sizer &sizer, RiskManager &riskManager)
      : signal_(signal), sizer_(sizer), riskManager_(riskManager) {
    maxLookback_ = std::max(
        {signal.maxLookback(), sizer.maxLookback(), riskManager.maxLookback()});
  }
  int getMove(State&, Bar& bar)




};

#endif
