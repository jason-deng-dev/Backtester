#pragma once

#include "analytics.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <vector>

template <typename T> class RollingWindow {
public:
  explicit RollingWindow(int maxS) : maxSize(maxS) {}
  void addVal(T val) {

    auto sizeBefore = dq.size();

    dq.push_back(val);
    sum += val;
    sumSquared += val * val;

    // on the first time we hit maxSize, set EMA to getSMA()
    if (sizeBefore == std::size_t(maxSize) - 1) {
      EMA = *getSMA();
    }

    if (dq.size() > std::size_t(maxSize)) {
      double x = 2.0 / (maxSize + 1);
      EMA = x * val + (1 - x) * EMA;
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
  T getPercentile(double p) const {
    std::vector<T> temp (dq.begin(), dq.end());
    std::sort(temp.begin(),temp.end());
    return percentile(temp,p);
  }

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
  T EMA{};
  std::deque<T> dq{};
  T sum{0};
  T sumSquared{0};
  int maxSize{};
};
