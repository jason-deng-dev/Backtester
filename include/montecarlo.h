#pragma once

#include "analytics.h"
#include "state.h"
#include <cstddef>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

enum class Regime { WARMUP, VOLATILE, CALM };
enum class RegimeSwitch { CalmToCalm, CalmToVol, VolToCalm, VolToVol };

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
  void sampleTrades(int seed, int i, const Analytics &analytics);
  void sampleTradesSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesCUDA(int n, int seed, const Analytics &analytics);

  void sampleTradesRegime(int seed, int i, const Analytics &analytics);

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

  void createTransitionMatrix();

  double getTransitionProbability(RegimeSwitch rs) const {
    if (!transitionMatrix.count(rs))
      throw std::invalid_argument("Regime switch not found");
    return transitionMatrix.at(rs);
  }
  /*-----------------------Regime Analytics-----------------------------*/

private:
  std::unordered_map<RegimeSwitch, int> transitionCountMap;
  std::unordered_map<RegimeSwitch, double> transitionMatrix;
  std::vector<sampleOutcome> outcomes;
  std::unordered_map<std::string, std::size_t> dateToIndexMap;
  std::vector<Regime> regimes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};
