#pragma once

#include "analytics.h"
#include "state.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
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

struct SampleOutcome {
  double balance;
  double maxDrawdown;
  double meanTradeReturn;
};

struct Distribution {

  void fill(std::vector<double> &values, double sum, double sumSquared) {
    if (values.empty()) {
      throw std::invalid_argument("empty values");
    }

    auto p5 = percentile(values, 0.05);
    auto p25 = percentile(values, 0.25);
    auto p50 = percentile(values, 0.50);
    auto p75 = percentile(values, 0.75);
    auto p95 = percentile(values, 0.95);
    int n = values.size();
    double mean = sum / n;
    double stdDev = std::sqrt((sumSquared - sum * sum / n) / (n - 1));
  }

  std::optional<double> p5{}, p25{}, p50{}, p75{}, p95{};
  double mean{}, std{};
};

struct AggregateStats {
  Distribution balanceDistribution;
  Distribution maxDrawdownDistribution;
  Distribution meanTradeReturDistribution;
  double probLoseMoney{};
  double probRuin{}; // maxDrawdown exceed 1
  std::unordered_map<double, int> propDrawdownExceed;
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

  // rejects the starting conditions every path-stat entry point needs
  void validateStartingState(double startingBalance) const;

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

  /*-----------------------Compute Stats-----------------------------*/
  void computePathStat(int n, double startingBalance);
  void computeAllPathStatSerial(double startingBalance);
  void computeAllPathStatParallel(double startingBalance);
  void computeAllPathStatGPU(double startingBalance);

  void
  computeAggregateStats(const std::vector<SampleOutcome> &outcomesToCompute);
  void reportAggregateStats() const;

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

  const std::vector<std::vector<SampledTrade>> &getSampledTrades() const {
    return sampledTrades;
  }

  const std::vector<SampleOutcome> &getOutcomes() const { return outcomes; }

private:
  AggregateStats aggregateStats;
  double startingBalance_ = 0;
  RegimeData regimeData{};
  std::vector<SampleOutcome> outcomes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};

template <typename Outcomes, typename Projection>
void fillDistribution(const Outcomes &outcomes, Projection project,
                      Distribution &distribution) {

  std::vector<double> values;
  values.reserve(outcomes.size());

  double sum{};
  double sumSquares{};

  for (const auto outcome : outcomes) {
    sum += outcome.maxDrawdown;
    sumSquares += outcome.maxDrawdown * outcome.maxDrawdown;
    values.push_back(outcome.maxDrawdown);
  }
  std::sort(values.begin(), values.end());
  distribution.fill(values, sum, sumSquares);
}
