#pragma once

#include "backtest.h"
#include "state.h"
#include <cmath>
#include <cstddef>
#include <optional>
#include <queue>

class Signal;
class Sizer;
class RiskManager;

class Strategy {
  Signal &signal_;
  Sizer &sizer_;
  RiskManager &riskManager_;

public:
  Strategy(Signal &signal, Sizer &sizer, RiskManager &riskManager)
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
    if (dq.size() > std::size_t(maxSize)) {
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
