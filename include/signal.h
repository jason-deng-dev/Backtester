#pragma once
#include "state.h"
#include "datafeed.h"

class Signal {
public:
  Signal(int minLookback = 0) : minLookback_(minLookback) {}
  virtual ~Signal() = default;
  virtual int generate(const State &state, const std::vector<Bar> &history) = 0;
  int getMinLookback() const { return minLookback_; }

private:
  int minLookback_{}; // called by generate to verify we have enough history
};


class BuyHoldSignal : public Signal {
  int generate(const State &state, const std::vector<Bar> &history) override {
    if (history.size() == 0)
      return 1;
    if (state.getTotalBars() == history.size() + 1) {
      return -1;
    }
    return 0;
  }
};
