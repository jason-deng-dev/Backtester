#pragma once

#include "analytics.h"
#include "state.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
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
    if (values.size() < 2) {
      throw std::invalid_argument("insufficient values");
    }

    p5 = percentile(values, 0.05);
    p25 = percentile(values, 0.25);
    p50 = percentile(values, 0.50);
    p75 = percentile(values, 0.75);
    p95 = percentile(values, 0.95);
    size_t n = values.size();
    mean = sum / n;
    std = std::sqrt((sumSquared - sum * sum / n) / (n - 1));
  }

  std::optional<double> p5{}, p25{}, p50{}, p75{}, p95{};
  double mean{}, std{};
};

struct AggregateStats {
  Distribution balanceDistribution;
  Distribution maxDrawdownDistribution;
  Distribution meanTradeReturnDistribution;
  double probLoseMoney{};
  double probRuin{}; // maxDrawdown exceed 1
  std::unordered_map<double, double> propDrawdownExceed;
  // paths aggregated, which is not necessarily outcomes.size(): the caller
  // may aggregate a vector it built itself
  std::size_t paths{};
};

struct RegimeData {
  std::unordered_map<RegimeSwitch, std::vector<double>> transitionPnls{};
  std::unordered_map<RegimeSwitch, int> transitionCountMap{};
  std::unordered_map<RegimeSwitch, double> transitionMatrix{};
  std::vector<Regime> regimes{};
  std::unordered_map<std::string, std::size_t> dateToIndexMap{};
  int calmCount = 0;
  int volCount = 0;

  Regime getRegime(const std::string &date) const { return regimes.at(dateToIndexMap.at(date)); }

  double getTransitionProb(RegimeSwitch rs) const {
    if (!transitionMatrix.count(rs)) throw std::invalid_argument("Regime switch not found");
    return transitionMatrix.at(rs);
  }

  double getRegimeProb(Regime r) const {
    if (calmCount + volCount == 0) {
      throw std::logic_error("no labeled regime data, call classifyRegime first");
    }
    if (r == Regime::WARMUP) {
      throw std::invalid_argument("Warmup regime not valid choice, choose volatile or calm");
    }

    if (r == Regime::VOLATILE)
      return double(volCount) / (calmCount + volCount);
    else
      return double(calmCount) / (calmCount + volCount);
  }
};

class MonteCarlo {
private:
  void setupRegime(int n, const Analytics &analytics, std::vector<PositionRecord> &calmPositions,
                   std::vector<PositionRecord> &volPositions);

  // rejects the starting conditions every path-stat entry point needs
  void validateStartingState() const;

public:
  /*-----------------------Sample Trades-----------------------------*/
  void sampleTrade(int i, int seed, const std::vector<PositionRecord> &positionRecords);
  void sampleTradesSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesGPU(int n, int seed, const Analytics &analytics);

  void sampleTradeRegime(int i, int seed, const std::vector<PositionRecord> &calmPositions,
                         const std::vector<PositionRecord> &volPositions);
  void sampleTradesRegimeSerial(int n, int seed, const Analytics &analytics);
  void sampleTradesRegimeParallel(int n, int seed, const Analytics &analytics);
  void sampleTradesRegimeGPU(int n, int seed, const Analytics &analytics);

  /*-----------------------Compute Stats-----------------------------*/
  void computePathStat(int n);
  void computeAllPathStatSerial();
  void computeAllPathStatParallel();
  void computeAllPathStatGPU();

  void computeAggregateStats(const std::vector<SampleOutcome> &outcomesToCompute);
  void reportAggregateStats(std::ostream &os = std::cout) const;
  const AggregateStats &getAggregateStats() const { return aggregateStats; }

  /*-----------------------Regime classification-----------------------------*/
  void classifyRegime(const State &state, double volPercentile = 0.75, double calmPercentile = 0.6,
                      int returnLookback = 20, int volLookback = 252);

  const RegimeData &getRegimeData() const { return regimeData; }

  Regime getRegime(const std::string &date) const { return regimeData.getRegime(date); }

  void createTransitionMatrix();

  double getTransitionProb(RegimeSwitch rs) const { return regimeData.getTransitionProb(rs); }

  const std::vector<std::vector<SampledTrade>> &getSampledTrades() const { return sampledTrades; }

  const std::vector<SampleOutcome> &getOutcomes() const { return outcomes; }

private:
  AggregateStats aggregateStats;
  RegimeData regimeData{};
  std::vector<SampleOutcome> outcomes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};

template <typename Outcomes, typename Projection>
void fillDistribution(const Outcomes &outcomes, Projection project, Distribution &distribution) {

  std::vector<double> values;
  values.reserve(outcomes.size());

  double sum{};
  double sumSquares{};

  for (const auto &outcome : outcomes) {
    const double value = project(outcome);
    sum += value;
    sumSquares += value * value;
    values.push_back(value);
  }
  std::sort(values.begin(), values.end());
  distribution.fill(values, sum, sumSquares);
}
