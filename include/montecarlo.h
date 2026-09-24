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
  /*========================Base Monte Carlo===============================*/

  /*-----------------------Sample Trades-----------------------------*/
  void sampleTrade(int seed, int i, const Analytics &analytics);
  void sampleTradesSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesCUDA(int n, int seed, const Analytics &analytics);

  /*-----------------------Compute Path Stats-----------------------------*/
  void computePathStat(int n, double startingBalance);
  void computeAllPathStatSerial(double startingBalance);
  void computeAllPathStatParallel(double startingBalance);
  void computeAllPathStatCUDA(double startingBalance);

  /*===================Regime Switching Monte Carlo==========================*/

  /*-----------------------Regime classification-----------------------------*/
  void classifyRegime(const State &state, double volPercentile = 0.75,
                      double calmPercentile = 0.6, int returnLookback = 20,
                      int volLookback = 252);

  Regime getRegime(const std::string &date) const {
    return regimes.at(dateToIndexMap.at(date));
  }

  /*

  */

private:
  std::vector<sampleOutcome> outcomes;
  std::unordered_map<std::string, std::size_t> dateToIndexMap;
  std::vector<Regime> regimes;
  std::vector<std::vector<SampledTrade>> sampledTrades;



};
