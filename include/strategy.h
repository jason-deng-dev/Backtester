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
    

    auto sizeBefore = dq.size();
    
    dq.push_back(val);
    sum += val;
    sumSquared += val * val;

    // on the first time we hit maxSize, set EMA to getSMA()
    if (sizeBefore == std::size_t(maxSize)-1) {
      EMA = *getSMA();
    }
    
    if (dq.size() > std::size_t(maxSize)) {
      double x = 2.0/(maxSize+1);
      EMA = x * val + (1-x) * EMA;
      T frontVal = dq.front();
      sum -= frontVal;
      sumSquared -= frontVal * frontVal;
      dq.pop_front();
    }
  }
  int getMaxSize() { return maxSize; }
  T size() const { return dq.size(); }
  T getSum() const { return sum; }
  T getSumSquared() const { return sumSquared; }

  // precondition is that caller ensured there are enough data points before
  std::optional<T> getStdDev() const {
    double N = dq.size();
    if (N < maxSize)
      return std::nullopt;
    return std::sqrt((sumSquared - sum * sum / N) / (N - 1));
  }

  std::optional<T> getSMA() {
    double N = dq.size();
    if (N < maxSize)
      return std::nullopt;
    return sum / maxSize;
  }

  std::optional<T> getEMA() {
    double N = dq.size();
    if (N < maxSize) 
      return std::nullopt;
    return EMA;
  }




private:
  T EMA {};
  std::deque<T> dq{};
  T sum{0};
  T sumSquared{0};
  int maxSize{};
};
