#pragma once

#include "analytics.h"
#include "state.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <random>
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

struct RegimeData {
  std::unordered_map<RegimeSwitch, std::vector<double>> transitionPnls{};
  std::unordered_map<RegimeSwitch, int> transitionCountMap{};
  std::unordered_map<RegimeSwitch, double> transitionMatrix{};
  std::vector<Regime> regimes{};
  std::unordered_map<std::string, std::size_t> dateToIndexMap{};
  int calmCount = 0;
  int volCount = 0;

  Regime getRegime(const std::string &date) const {
    return regimes.at(dateToIndexMap.at(date));
  }

  double getTransitionProb(RegimeSwitch rs) const {
    if (!transitionMatrix.count(rs))
      throw std::invalid_argument("Regime switch not found");
    return transitionMatrix.at(rs);
  }

  double getRegimeProb(Regime r) const {
    if (calmCount + volCount == 0) {
      throw std::logic_error(
          "no labeled regime data, call classifyRegime first");
    }
    if (r == Regime::WARMUP) {
      throw std::invalid_argument(
          "Warmup regime not valid choice, choose volatile or calm");
    }

    if (r == Regime::VOLATILE)
      return double(volCount) / (calmCount + volCount);
    else
      return double(calmCount) / (calmCount + volCount);
  }
};

class MonteCarlo {
private:
  void setupRegime(int n, const Analytics &analytics,
                   std::vector<PositionRecord> &calmPositions,
                   std::vector<PositionRecord> &volPositions);

public:
  /*-----------------------Sample Trades-----------------------------*/
  void sampleTrade(int i, int seed,
                   const std::vector<PositionRecord> &positionRecords);
  void sampleTradesSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesGPU(int n, int seed, const Analytics &analytics);

  void sampleTradeRegime(int i, int seed,
                         const std::vector<PositionRecord> &calmPositions,
                         const std::vector<PositionRecord> &volPositions);
  void sampleTradesRegimeSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesRegimeParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesRegimeGPU(int n, int seed, const Analytics &analytics);

  /*-----------------------Compute Path Stats-----------------------------*/
  void computePathStat(int n, double startingBalance);
  void computeAllPathStatSerial(double startingBalance);
  void computeAllPathStatParallel(double startingBalance);
  void computeAllPathStatGPU(double startingBalance);

  /*-----------------------Regime classification-----------------------------*/
  void classifyRegime(const State &state, double volPercentile = 0.75,
                      double calmPercentile = 0.6, int returnLookback = 20,
                      int volLookback = 252);

  const RegimeData &getRegimeData() const { return regimeData; }

  Regime getRegime(const std::string &date) const {
    return regimeData.getRegime(date);
  }

  void createTransitionMatrix();

  double getTransitionProb(RegimeSwitch rs) const {
    return regimeData.getTransitionProb(rs);
  }

private:
  RegimeData regimeData{};
  std::vector<sampleOutcome> outcomes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};
