#pragma once

#include "analytics.h"
#include "state.h"
#include <cstddef>
#include <cstdlib>
#include <string>
#include <unordered_map>
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

  void computeAllPathStatSerial(double startingBalance);

  void computeAllPathStatParallel(double startingBalance);

  void computePathStat(int n, double startingBalance);

  void classifyRegime(const State &state, double volPercentile = 0.75,
                      double calmPercentile = 0.6, int returnLookback = 20,
                      int volLookback = 252);

  Regime getRegime(const std::string &date) const {
    return regimes.at(dateToIndexMap.at(date));
  }

private:
  std::vector<sampleOutcome> outcomes;
  std::unordered_map<std::string, std::size_t> dateToIndexMap;
  std::vector<Regime> regimes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};
