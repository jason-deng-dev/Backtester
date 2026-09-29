#include "montecarlo.h"
#include "analytics.h"
#include "rollingwindow.h"
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <thread>
#include <utility>

/*-----------------------Regime Classificaiton-----------------------------*/

void MonteCarlo::classifyRegime(const State &state, double volPercentile,
                                double calmPercentile, int returnLookback,
                                int volLookback) {

  // reset regimeData before running
  regimeData = {};
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
    regimeData.dateToIndexMap[bar.date] = index++;

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
      regimeData.regimes.push_back(Regime::WARMUP);
    } else {

      if (currState == Regime::CALM && *currVol > *currHigh) {
        if (hasLabeled)
          regimeData.transitionCountMap[RegimeSwitch::CalmToVol]++;
        currState = Regime::VOLATILE;
      } else if (currState == Regime::VOLATILE && *currVol < *currLow) {
        regimeData.transitionCountMap[RegimeSwitch::VolToCalm]++;
        currState = Regime::CALM;
      } else {
        if (currState == Regime::CALM && hasLabeled)
          regimeData.transitionCountMap[RegimeSwitch::CalmToCalm]++;
        if (currState == Regime::VOLATILE) {
          regimeData.transitionCountMap[RegimeSwitch::VolToVol]++;
        }
      }
      if (currState == Regime::CALM)
        ++regimeData.calmCount;
      else if (currState == Regime::VOLATILE)
        ++regimeData.volCount;

      hasLabeled = true;
      regimeData.regimes.push_back(currState);
    }
  }

  createTransitionMatrix();
}

void MonteCarlo::createTransitionMatrix() {
  auto &transitionCountMap = regimeData.transitionCountMap;
  auto &transitionMatrix = regimeData.transitionMatrix;

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

void MonteCarlo::sampleTrade(
    int i, int seed, const std::vector<PositionRecord> &positionRecords) {
  // keep sampling until fill until reach
  std::vector<SampledTrade> tradePath;
  auto recordSize = positionRecords.size();

  tradePath.reserve(recordSize);

  std::mt19937 gen(seed + i);
  std::uniform_int_distribution<std::size_t> dist(0, recordSize - 1);
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
    sampleTrade(i, seed, positionRecords);
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
        sampleTrade(i, seed, positionRecords);
      }
    });
  }
  for (auto &th : threads)
    th.join();
}

void MonteCarlo ::sampleTradeRegime(
    int i, int seed, const std::vector<PositionRecord> &calmPositions,
    const std::vector<PositionRecord> &volPositions) {
  /*
Randomly select starting regime : currRegime
Sample from positionRecords where regime = currRegime
use transition matrix to select next regime
*/
  std::vector<SampledTrade> tradePath;

  auto calmSize = calmPositions.size();
  auto volSize = volPositions.size();
  auto recordSize = calmSize + volSize;

  if (calmSize == 0 || volSize == 0)
    throw std::logic_error("sampleTradeRegime: empty regime bucket");

  std::mt19937 gen(seed + i);
  std::uniform_int_distribution<std::size_t> calmDist(0, calmSize - 1);
  std::uniform_int_distribution<std::size_t> volDist(0, volSize - 1);

  double volProb = regimeData.getRegimeProb(Regime::VOLATILE);
  std::bernoulli_distribution startVolDist(volProb);
  Regime currRegime = startVolDist(gen) ? Regime::VOLATILE : Regime::CALM;

  std::bernoulli_distribution volToVolDist(
      regimeData.getTransitionProb(RegimeSwitch::VolToVol));
  std::bernoulli_distribution calmToCalmDist(
      regimeData.getTransitionProb(RegimeSwitch::CalmToCalm));

  while (tradePath.size() < recordSize) {
    auto &position = currRegime == Regime::VOLATILE
                         ? volPositions[volDist(gen)]
                         : calmPositions[calmDist(gen)];

    tradePath.push_back(
        {position.pnl / std::abs(position.entryNotional), currRegime});

    if (currRegime == Regime::VOLATILE && !volToVolDist(gen)) {
      currRegime = Regime::CALM;
    } else if (currRegime == Regime::CALM && !calmToCalmDist(gen)) {
      currRegime = Regime::VOLATILE;
    }
  }
  sampledTrades[i] = std::move(tradePath);
}

void MonteCarlo::sampleTradesRegimeSerial(int n, int seed,
                                          const Analytics &analytics) {
  std::vector<PositionRecord> calmPositions;
  std::vector<PositionRecord> volPositions;
  setupRegime(n, analytics, calmPositions, volPositions);

  for (int i = 0; i < n; ++i) {
    sampleTradeRegime(i, seed, calmPositions, volPositions);
  }
}

void MonteCarlo::sampleTradesRegimeParallel(int n, int seed,
                                            const Analytics &analytics) {
  std::vector<PositionRecord> calmPositions;
  std::vector<PositionRecord> volPositions;
  setupRegime(n, analytics, calmPositions, volPositions);

  unsigned hw = std::thread::hardware_concurrency();
  auto num_threads = std::min(hw != 0 ? hw : 1, static_cast<unsigned>(n));

  std::vector<std::thread> threads;
  for (unsigned t = 0; t < num_threads; ++t) {
    threads.emplace_back([&, t] {
      for (size_t i = t; i < n; i += num_threads) {
        sampleTradeRegime(i, seed, calmPositions, volPositions);
      }
    });
  }
  for (auto &th : threads)
    th.join();
}

void MonteCarlo::setupRegime(int n, const Analytics &analytics,
                             std::vector<PositionRecord> &calmPositions,
                             std::vector<PositionRecord> &volPositions) {
  if (n == 0) {
    throw std::logic_error("Can't sample 0 trades");
  }
  sampledTrades.resize(n);

  const auto &positionRecords = analytics.getPositionRecords();

  for (auto &pos : positionRecords) {
    // note by design, trade is tagged with regime based on entry time
    if (getRegime(pos.openTime) == Regime::CALM) {
      calmPositions.push_back(pos);
    } else if (getRegime(pos.openTime) == Regime::VOLATILE) {
      volPositions.push_back(pos);
    }
  }
}

/*-----------------------Compute Path Stats-----------------------------*/
// shared by the serial and parallel entry points; must run before any thread
// is spawned, since a throw from inside a worker terminates the process
void MonteCarlo::validateStartingState(double startingBalance) const {
  if (startingBalance <= 0.0) {
    throw std::invalid_argument("startingBalance must be positive");
  }
  if (sampledTrades.empty()) {
    throw std::logic_error("sampledTrades is empty");
  }
  if (std::any_of(sampledTrades.begin(), sampledTrades.end(),
                  [](const auto &path) { return path.empty(); })) {
    throw std::logic_error("empty sampled trade path");
  }
}

void MonteCarlo::computePathStat(int n, double startingBalance) {
  double balance = startingBalance;
  double maxDrawDown{0}; // Drawdown_t = (Peak_t-V_t)/Peak_t
  double peak{balance};

  auto &tradePath = sampledTrades[n];

  double sumReturns{0};

  for (auto &trade : tradePath) {
    balance += balance * trade.fractionalReturn;
    peak = std::max(peak, balance);
    maxDrawDown = std::max(maxDrawDown, (peak - balance) / peak);
    sumReturns += trade.fractionalReturn;
  }

  outcomes[n] = {balance, maxDrawDown, sumReturns / tradePath.size()};
}

void MonteCarlo::computeAllPathStatSerial(double startingBalance) {
  validateStartingState(startingBalance);

  startingBalance_ = startingBalance;
  const std::size_t size = sampledTrades.size();
  outcomes.resize(size);

  for (std::size_t i = 0; i < size; ++i) {
    computePathStat(i, startingBalance);
  }
}

void MonteCarlo::computeAllPathStatParallel(double startingBalance) {
  validateStartingState(startingBalance);

  startingBalance_ = startingBalance;
  const std::size_t n = sampledTrades.size();
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

// allow passing in std::vector<SampleOutcome>* outcomes for testing
void MonteCarlo::computeAggregateStats(
    const std::vector<SampleOutcome> &outcomesToCompute) {
  int n = outcomesToCompute.size();
  double total = n;
  if (total == 0) {
    throw std::logic_error("outcomes is empty");
  }

  int loseMoney = 0, MDD10 = 0, MDD20 = 0, MDD30 = 0, MDD50 = 0, ruin = 0;

  for (const auto &sampleOutcome : outcomesToCompute) {
    int balance = sampleOutcome.balance;
    int mdd = sampleOutcome.maxDrawdown;
    if (balance < startingBalance_) ++loseMoney;
    if (mdd >= 0.1) ++MDD10;
    if (mdd >= 0.2) ++MDD20;
    if (mdd >= 0.3) ++MDD30;
    if (mdd >= 0.5) ++MDD50;
    if (mdd >= 1) {
      ++ruin;
    }
  }

  aggregateStats.probLoseMoney = loseMoney / total;
  aggregateStats.probRuin = ruin / total;
  aggregateStats.propDrawdownExceed[0.1] = MDD10 / total;
  aggregateStats.propDrawdownExceed[0.2] = MDD20 / total;
  aggregateStats.propDrawdownExceed[0.3] = MDD30 / total;
  aggregateStats.propDrawdownExceed[0.5] = MDD50 / total;

  fillDistribution(
      outcomesToCompute, [](const SampleOutcome &x) { return x.maxDrawdown; },
      aggregateStats.maxDrawdownDistribution);

  fillDistribution(
      outcomesToCompute, [](const SampleOutcome &x) { return x.balance; },
      aggregateStats.balanceDistribution);

  fillDistribution(
      outcomesToCompute,
      [](const SampleOutcome &x) { return x.meanTradeReturn; },
      aggregateStats.meanTradeReturDistribution);
}

void MonteCarlo::reportAggregateStats() const {}
