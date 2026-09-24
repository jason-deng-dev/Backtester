#pragma once

#include "analytics.h"
#include "rollingwindow.h"
#include "state.h"
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

enum class Regime { WARMUP, VOLATILE, CALM };

struct SampledTrade {
  double fractionalReturn;
  Regime regime;
};

struct sampleOutcome {
  double peak;
  double trough;
  double balance;
  double maxDrawdown;
};

class MonteCarlo {
public:
  void sampleTrade(int seed, int i, const Analytics &analytics);

  // ground-truth reference
  void sampleTradesSerial(int n, int seed, const Analytics &analytics);

  // CPU multithreaded
  void sampleTradesParallel(int n, int seed, const Analytics &analytics);

  // GPU
  void sampleTradesCUDA(int n, int seed, const Analytics &analytics);

  void computeAllPathStatSerial(int startingBalance);

  void computePathStat(int n, int startingBalance);

  void classifyRegime(const State &state, double volPercentile = 0.75,
                      double calmPercentile = 0.6, int returnLookback = 20,
                      int volLookback = 252);

  Regime getRegime(const std::string &date) const {
    return regimes.at(dateToIndexMap.at(date));
  }

private:
  std::vector<Outcome> outcomes;
  std::unordered_map<std::string, std::size_t> dateToIndexMap;
  std::vector<Regime> regimes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};
