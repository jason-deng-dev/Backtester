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

/*-----------------------Regime Classificaiton-----------------------------*/

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
  double prevEquity{};

  // handles first phantom transition (before have prior state)
  bool hasLabeled = false;

  for (auto &bar : bars) {
    dateToIndexMap[bar.date] = index++;

    // returns are close-to-close between real bars; barSnapshots[0] is the
    // "before trading" sentinel, not a market observation
    if (index > 2 && prevEquity != 0) {
      returnWindow.addVal(bar.equity / prevEquity - 1.0);
      if (const auto vol = returnWindow.getStdDev()) {
        volWindow.addVal(*vol);
      }
    }
    prevEquity = bar.equity;

    const auto currVol = returnWindow.getStdDev();
    const auto currHigh = volWindow.getPercentile(volPercentile);
    const auto currLow = volWindow.getPercentile(calmPercentile);

    if (!currVol || !currHigh || !currLow) {
      regimes.push_back(Regime::WARMUP);
    } else {

      if (currState == Regime::CALM && *currVol > *currHigh) {
        if (hasLabeled)
          transitionCountMap[RegimeSwitch::CalmToVol]++;
        currState = Regime::VOLATILE;
      } else if (currState == Regime::VOLATILE && *currVol < *currLow) {
        transitionCountMap[RegimeSwitch::VolToCalm]++;
        currState = Regime::CALM;
      } else {
        if (currState == Regime::CALM && hasLabeled)
          transitionCountMap[RegimeSwitch::CalmToCalm]++;
        if (currState == Regime::VOLATILE) {
          transitionCountMap[RegimeSwitch::VolToVol]++;
        }
      }
      hasLabeled = true;
      regimes.push_back(currState);
    }
  }
}

void MonteCarlo::createTransitionMatrix() {
  int totalCalm = transitionCountMap[RegimeSwitch::CalmToCalm] +
                  transitionCountMap[RegimeSwitch::CalmToVol];
  int totalVol = transitionCountMap[RegimeSwitch::VolToVol] +
                 transitionCountMap[RegimeSwitch::VolToCalm];
  transitionMatrix[RegimeSwitch::CalmToCalm] =
      transitionCountMap[RegimeSwitch::CalmToCalm] / double(totalCalm);
  transitionMatrix[RegimeSwitch::CalmToVol] =
      transitionCountMap[RegimeSwitch::CalmToVol] / double(totalCalm);
  transitionMatrix[RegimeSwitch::VolToCalm] =
      transitionCountMap[RegimeSwitch::VolToCalm] / double(totalVol);
  transitionMatrix[RegimeSwitch::VolToVol] =
      transitionCountMap[RegimeSwitch::VolToVol] / double(totalVol);
}

/*-----------------------Sample Trades-----------------------------*/

void MonteCarlo::sampleTrade(int seed, int i, const std::vector<PositionRecord>& positionRecords) {
  // keep sampling until fill until reach
  std::vector<SampledTrade> tradePath;
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
  const auto &positionRecords = analytics.getPositionRecords();
  sampledTrades.resize(n);
  for (int i = 0; i < n; ++i) {
    sampleTrade(seed, i, positionRecords);
  }
}

void MonteCarlo::sampleTradesParallel(int n, int seed,
                                      const Analytics &analytics) {
  if (n == 0) {
    throw std::logic_error("Can't sample 0 trades");
  }

  const auto &positionRecords = analytics.getPositionRecords();

  sampledTrades.resize(n);

  unsigned hw = std::thread::hardware_concurrency();
  auto num_threads = std::min(hw != 0 ? hw : 1, static_cast<unsigned>(n));

  std::vector<std::thread> threads;
  for (unsigned t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = t; i < n; i += num_threads) {
        sampleTrade(seed, i, positionRecords);
      }
    });
  }
  for (auto &th : threads)
    th.join();
}

void MonteCarlo ::sampleTradeRegime(int seed, int i,
                                     const Analytics &analytics) {
  std::vector<SampledTrade> tradePath;

  const auto& positionRecords = analytics.getPositionRecords();
  
  std::vector<PositionRecord> calmPositions;
  std::vector<PositionRecord> volPositions;

  for (auto& pos: positionRecords) {
    if (pos.openTime)
  }


  /*
  Randomly select starting regime : currRegime
  Sample from positionRecords where regime = currRegime
  use transition matrix to select next regime
  */
}

void MonteCarlo::sampleTradesRegimeSerial(int n, int seed, const Analytics& analytics) {

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
  int n = sampledTrades.size();
  if (n == 0) {
    throw std::logic_error("sampledTrades is empty");
  }
  outcomes.resize(n);

  unsigned hw = std::thread::hardware_concurrency();
  auto num_threads = std::min(hw != 0 ? hw : 1, static_cast<unsigned>(n));
  std::vector<std::thread> threads;

  for (unsigned t = 0; t < num_threads; ++t) { // for each thread
    threads.emplace_back([&, t] {
      for (size_t i = t; i < n;
           i += num_threads) { // evenly distribute work amoung them
        computePathStat(i, startingBalance);
      }
    });
  }
  for (auto &th : threads) {
    th.join();
  }
}
