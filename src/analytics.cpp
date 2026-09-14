#include "analytics.h"
#include "state.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <numeric>
#include <optional>

bool Analytics::captureState(const State &state) {
  // BarSnapshot<date, equity, netQty, minPrice, maxPrice>
  const auto &barSnapshots = state.getBarSnapshots();
  // Execution<date, qty, price>
  const auto &executions = state.getExecutions();

  if (barSnapshots.empty()) {
    return false;
  }

  recordInfo(executions, barSnapshots);

  return true;
}

// precondition: barSnapshots size > 0
void Analytics::recordInfo(const std::vector<Execution> &executions,
                           const std::vector<BarSnapshot> &barSnapshots) {

  tradingDays = barSnapshots.size() - 1;
  startEquity = barSnapshots[0].equity;
  endEquity = barSnapshots.back().equity;

  int executionIndex = 0;
  double prevEquity = barSnapshots[0].equity;

  // execution handling

  EpisodeState episodeState{};

  for (std::size_t i = 0; i < barSnapshots.size(); ++i) {
    auto [date, equity, netQty, minPrice, maxPrice] = barSnapshots[i];
    if (netQty != 0)
      ++daysInMarket;

    runningPeak = std::max(equity, runningPeak);
    maxDrawDown = std::max(maxDrawDown, (runningPeak - equity) / runningPeak);

    if (i > 0) {
      sharpeInfo.dailyReturns.push_back((equity - prevEquity) / prevEquity);
    }

    prevEquity = equity;

    while (executionIndex < executions.size() &&
           date == executions[executionIndex].date) {
      handleExecution(executions[executionIndex], episodeState);
      ++executionIndex;
    }

    if (netQty != 0) {
      const double favorable = netQty * ((netQty > 0 ? maxPrice : minPrice) -
                                         episodeState.avgEntryPrice);
      const double adverse = netQty * ((netQty > 0 ? minPrice : maxPrice) -
                                       episodeState.avgEntryPrice);

      episodeState.mfe = std::max(favorable, episodeState.mfe);
      episodeState.mae = std::min(adverse, episodeState.mae);
    }
  }
}

auto sign = [](int q) { return (q > 0) - (q < 0); };

void Analytics::handleExecution(const Execution &execution,
                                EpisodeState &episodeState) {
  auto [date, qty, price] = execution;
  auto &[openTime, openPositions, entryNotional, exitNotional, avgEntryPrice,
         mae, mfe] = episodeState;

  int remaining = std::abs(qty);
  const bool wasOpen = !openPositions.empty();
  // direction of the position held during this fill
  // (on a flip this is the direction of the position being closed)
  const int posDir = wasOpen ? openPositions.front().direction : sign(qty);

  // consume open lots FIFO while this fill opposes the position
  while (remaining != 0 && !openPositions.empty() &&
         sign(qty) != openPositions.front().direction) {
    OpenPosition &lot = openPositions.front();
    const int matched = std::min(remaining, lot.qty);
    const double pnl = lot.direction * matched * (price - lot.price);

    exitRecords.push_back(
        {lot.date, date, lot.direction, matched, price, lot.price, pnl});
    exitNotional += lot.direction * matched * price;
    remaining -= matched;

    if (lot.qty == matched) {
      openPositions.pop_front();
    } else {
      lot.qty -= matched;
    }
  }

  // this fill drained the position: episode closes
  if (wasOpen && openPositions.empty()) {
    positionRecords.push_back({openTime, date, posDir, entryNotional,
                               exitNotional, exitNotional - entryNotional, mae,
                               mfe});
    entryNotional = 0;
    exitNotional = 0;
    mae = std::numeric_limits<double>::infinity();
    mfe = -std::numeric_limits<double>::infinity();
  }

  // remainder opens a new position or extends the current one
  if (remaining != 0) {
    if (openPositions.empty()) {
      openTime = date;
    }

    int qtyBefore = 0;
    for (const auto &lot : openPositions) {
      qtyBefore += lot.qty;
    }

    avgEntryPrice = (avgEntryPrice * qtyBefore + price * remaining) /
                    (qtyBefore + remaining);

    openPositions.push_back({date, sign(qty), remaining, price});
    entryNotional += sign(qty) * remaining * price;
  }
}

bool Analytics::computeSharpe() {
  int n = sharpeInfo.dailyReturns.size();
  if (n < 2)
    return false;
  sharpeInfo.meanDailyReturns =
      std::accumulate(sharpeInfo.dailyReturns.begin(),
                      sharpeInfo.dailyReturns.end(), 0.0) /
      n;
  double rfDaily = std::pow((1 + sharpeInfo.riskFreeAnnual), 1.0 / 252) - 1;

  sharpeInfo.variance = std::accumulate(
      sharpeInfo.dailyReturns.begin(), sharpeInfo.dailyReturns.end(), 0.0,
      [=](double acc, double curr) {
        return acc + (curr - sharpeInfo.meanDailyReturns) *
                         (curr - sharpeInfo.meanDailyReturns) / (n - 1);
      });

  sharpeInfo.sharpe = (sharpeInfo.meanDailyReturns - rfDaily) /
                      std::sqrt(sharpeInfo.variance) * std::sqrt(252);

  return true;
}

void Analytics::computePositionRecords() {
  positionInfo.num = positionRecords.size();

  // for each position, ++numWinningPositions, +=
  for (const auto &pr : positionRecords) {
    bool win = pr.pnl > 0;
    bool isLong = pr.direction > 0;
    excursions.push_back({pr.mae / std::abs(pr.entryNotional),
                          pr.mfe / std::abs(pr.entryNotional), isLong, win});

    if (win) {
      positionInfo.numWin++;
      positionInfo.grossProfit += pr.pnl;
    } else
      positionInfo.grossLoss += pr.pnl;
  }
}

void Analytics::computeExitRecords() {
  exitsInfo.num = exitRecords.size();
  for (const auto &ex : exitRecords) {
    if (ex.pnl > 0) {
      exitsInfo.numWin++;
      exitsInfo.grossProfit += ex.pnl;
    } else {
      exitsInfo.grossLoss += ex.pnl;
    }
  }
}

std::vector<Execursion> filter(const std::vector<Execursion> &excursions,
                               double isLong, bool win) {
  std::vector<Execursion> v;
  std::copy_if(
      excursions.begin(), excursions.end(), std::back_inserter(v),
      [=](Execursion ex) { return ex.isLong == isLong && ex.win == win; });
  return v;
}

// mode = "mae" / "mfe"
// precondition that v is sorted based on mode before passing
std::optional<double> median(const std::vector<Execursion> &v, bool mae) {
  if (v.empty())
    return std::nullopt;
  std::size_t n = v.size();
  if (mae) {
    return (n % 2) ? v[n / 2].maeNorm
                   : 0.5 * (v[n / 2 - 1].maeNorm + v[n / 2].maeNorm);
  } else {
    return (n % 2) ? v[n / 2].mfeNorm
                   : 0.5 * (v[n / 2 - 1].mfeNorm + v[n / 2].mfeNorm);
  }
}

// mode = "mae" / "mfe"
// precondition that v is sorted based on mode before passing
// enforce minNeeded so we have statisically valuable percentaile result
std::optional<double> percentaile(const std::vector<Execursion> &v, double p,
                                  bool mae) {
  if (p < 0 || p > 1)
    return std::nullopt;

  std::size_t minNeeded = std::ceil(1.0 / (1.0 - p)) * 2;
  std::size_t n = v.size();
  if (n < minNeeded)
    return std::nullopt;

  std::size_t k = std::ceil(p * n);
  if (k < 1)
    k = 1;
  if (k > n)
    k = n;

  if (mae)
    return v[k - 1].maeNorm;
  else
    return v[k - 1].mfeNorm;
}
