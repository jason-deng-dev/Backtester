#pragma once

#include "analytics.h"
#include "rollingwindow.h"
#include "state.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <random>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class Regime { WARMUP, VOLATILE, CALM };

struct SampledTrade {
  double pnl;
  Regime regime;
};

class MonteCarlo {
public:

  void sampleNTrades(int n, int seed) {
    for ()

  }

  void sampleTrade(int seed, const Analytics &analytics) {
    // keep sampling until fill until reach
    std::vector<SampledTrade> tradePath;
    
    const auto &positionRecords = analytics.getPositionRecords();
    auto recordSize = positionRecords.size();

    tradePath.reserve(recordSize);

    std::mt19937 gen(seed);
    std::uniform_int_distribution<std::size_t> dist(0,
                                                    positionRecords.size() - 1);
    while (tradePath.size() < recordSize) {
      auto &position = positionRecords[dist(gen)];
      tradePath.push_back({position.pnl, getRegime(position.openTime)});
    }

    // move to avoid unneeded copy
    sampledTrades.push_back(std::move(tradePath));
  }

  void classifyRegime(const State &state, double volPercentile = 0.75,
                      double calmPercentile = 0.6, int returnLookback = 20,
                      int volLookback = 252) {

    // need std dev
    RollingWindow<double> returnWindow{returnLookback};

    // need way to quickly calculate percentile from it
    RollingWindow<double> volWindow{volLookback};

    const auto &bars = state.getBarSnapshots();

    Regime currState = Regime::CALM;
    int index = 0;

    for (auto &bar : bars) {
      dateToIndexMap[bar.date] = index++;
      const auto currVol = returnWindow.getStdDev();
      const auto currHigh = volWindow.getPercentile(volPercentile);
      const auto currLow = volWindow.getPercentile(calmPercentile);

      if (!currVol || !currHigh) {
        regimes.push_back(Regime::WARMUP);
      } else {
        if (currState == Regime::CALM && *currVol > *currHigh) {
          currState = Regime::VOLATILE;
        } else if (currState == Regime::VOLATILE && *currVol < *currLow) {
          currState = Regime::CALM;
        }
        regimes.push_back(currState);
      }
    }
  }

  Regime getRegime(const std::string &date) const {
    return regimes.at(dateToIndexMap.at(date));
  }

private:
  std::unordered_map<std::string, std::size_t> dateToIndexMap;
  std::vector<Regime> regimes;
  std::vector<std::vector<SampledTrade>> sampledTrades;
};
