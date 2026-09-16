#pragma once

#include "backtest.h"
#include "state.h"
#include <cmath>
#include <cstddef>
#include <optional>
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

class Signal;
class Sizer;
class RiskManager;

class StrategyImproved {
  Signal &signal_;
  Sizer &sizer_;
  RiskManager &riskManager_;

public:
  StrategyImproved(Signal &signal, Sizer &sizer, RiskManager &riskManager)
      : signal_(signal), sizer_(sizer), riskManager_(riskManager) {}
  int getMove(const State &state, const std::vector<Bar> &history);
};

template <typename T> class RollingWindow {
public:
  explicit RollingWindow(int maxS) : maxSize(maxS) {}
  virtual void addVal(T val) {
    dq.push_back(val);
    sum += val;
    sumSquared += val * val;
    if (dq.size() > maxSize) {
      T frontVal = dq.front();
      sum -= frontVal;
      sumSquared -= frontVal * frontVal;
      dq.pop_front();
    }
  }
  T size() const { return dq.size(); }
  T getSum() const { return sum; }
  T getSumSquared() const { return sumSquared; }

  // precondition is that caller ensured there are enough data points before
  std::optional<T> getStdDev() const {
    double N = dq.size();
    if (N < maxSize)
      return {};
    return std::sqrt((sumSquared - sum * sum / N) / (N - 1));
  }

private:
  std::deque<T> dq{};
  T sum{0};
  T sumSquared{0};
  int maxSize{};
};
