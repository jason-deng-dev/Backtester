#include "montecarlo.h"
#include "analytics.h"
#include "rollingwindow.h"
#include <algorithm>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>

void MonteCarlo::classifyRegime(const State &state, double volPercentile,
                                double calmPercentile, int returnLookback,
                                int volLookback) {

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

/*-----------------------Sample Trades-----------------------------*/

void MonteCarlo::sampleTrade(int seed, int i, const Analytics &analytics) {
  // keep sampling until fill until reach
  std::vector<SampledTrade> tradePath;

  const auto &positionRecords = analytics.getPositionRecords();
  auto recordSize = positionRecords.size();

  tradePath.reserve(recordSize);

  std::mt19937 gen(seed + i);
  std::uniform_int_distribution<std::size_t> dist(0,
                                                  positionRecords.size() - 1);
  while (tradePath.size() < recordSize) {
    auto &position = positionRecords[dist(gen)];
    tradePath.push_back({position.pnl / std::abs(position.entryNotional),
                         getRegime(position.openTime)});
  }

  // move to avoid unneeded copy
  sampledTrades[i] = std::move(tradePath);
}

void MonteCarlo::sampleTradesSerial(int n, int seed,
                                    const Analytics &analytics) {
  if (n == 0) {
    throw std::logic_error("Can't sample 0 trades");
  }
  sampledTrades.resize(n);
  for (int i = 0; i < n; ++i) {
    sampleTrade(seed, i, analytics);
  }
}

void MonteCarlo::sampleTradesParallel(int n, int seed,
                                      const Analytics &analytics) {
  if (n == 0) {
    throw std::logic_error("Can't sample 0 trades");
  }
  sampledTrades.resize(n);
  auto C = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < C; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = t; i < n; i += C) {
        sampleTrade(seed, i, analytics);
      }
    });
  }
  for (auto &th : threads)
    th.join();
}

/*-----------------------Compute Path Stats-----------------------------*/
void MonteCarlo::computePathStat(int n, double startingBalance) {
  double balance = startingBalance;

  double maxDrawDown{0}; // Drawdown_t = (Peak_t-V_t)/Peak_t
  double peak{balance};
  double trough{balance};

  auto &tradePath = sampledTrades[n];

  for (auto &trade : tradePath) {
    balance += balance * trade.fractionalReturn;
    peak = std::max(peak, balance);
    trough = std::min(trough, balance);
    maxDrawDown = std::max(maxDrawDown, (peak - balance) / peak);
  }

  outcomes[n] = {peak, trough, balance, maxDrawDown};
}

void MonteCarlo::computeAllPathStatSerial(double startingBalance) {
  int size = sampledTrades.size();
  if (size == 0) {
    throw std::logic_error("sampledTrades is empty");
  }
  outcomes.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    computePathStat(i, startingBalance);
  }
}

void MonteCarlo::computeAllPathStatParallel(double startingBalance) {
  int size = sampledTrades.size();
  if (size == 0) {
    throw std::logic_error("sampledTrades is empty");
  }
  outcomes.resize(size);

  auto C = std::thread::hardware_concurrency();
  std::vector<std::thread> threads;
  for (unsigned t = 0; t < C; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = t; i < size; i += C) {
        computePathStat(i, startingBalance);
      }
    });
  }
  for (auto &th : threads) {
    th.join();
  }
}
