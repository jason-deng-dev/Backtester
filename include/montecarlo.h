#pragma once

#include "rollingwindow.h"
#include "state.h"
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

enum class Regime { WARMUP, VOLATILE, CALM };

class MonteCarlo {
public:
  MonteCarlo(std::mt19937 &gen) : gen_(gen) {}

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
  std::mt19937 &gen_;
};
