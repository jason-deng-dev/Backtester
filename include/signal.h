#pragma once
#include "datafeed.h"
#include "state.h"
#include <iostream>
#include <random>
#include <stdexcept>

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
    if (history.size() == 1) {
      return 1;
    }

    if (state.getTotalBars() == history.size() + 1) {
      return -1;
    }
    return 0;
  }
};

static double checked(double p) {
  if (p < 0.0 || p > 1.0 || std::isnan(p))
    throw std::invalid_argument("probability must be in [0,1]");
  return p;
}

class RandomSignal : public Signal {
public:
  explicit RandomSignal(double pEnter, double pExit, double pLong,
                        std::mt19937 &gen)
      : gen_(gen), enterDist_(std::bernoulli_distribution(checked(pEnter))),
        exitDist_(std::bernoulli_distribution(checked(pExit))),
        longDist_(std::bernoulli_distribution(checked(pLong))) {
    if (pEnter < 0 || pEnter > 1 || pLong < 0 || pLong > 1 || pExit < 0 ||
        pExit > 1)
      throw std::invalid_argument("pEnter, pExit and pLong must be in [0,1]");
  }

  int generate(const State &state, const std::vector<Bar> &history) override {
    auto netQty = state.getNetQty();
    if (netQty == 0) {
      if (!enterDist_(gen_))
        return 0;

      if (!longDist_(gen_)) {
        return -1;
      } else {
        return 1;
      }
    } else {
      if (!exitDist_(gen_)) {
        return 0;
      }
      if (netQty > 0)
        return -1;
      else
        return 1;
    }
  }

private:
  std::mt19937 &gen_;
  std::bernoulli_distribution enterDist_;
  std::bernoulli_distribution exitDist_;
  std::bernoulli_distribution longDist_;
};
