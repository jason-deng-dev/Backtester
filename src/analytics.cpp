#include "analytics.h"
#include "state.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>

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
      const double favorable =
          netQty * ((netQty > 0 ? maxPrice : minPrice) - episodeState.avgEntryPrice);
      const double adverse =
          netQty * ((netQty > 0 ? minPrice : maxPrice) - episodeState.avgEntryPrice);

      episodeState.mfe = std::max(favorable, episodeState.mfe);
      episodeState.mae = std::min(adverse, episodeState.mae);
    }

  }
}

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
  if (n<2) return false;
  sharpeInfo.meanDailyReturns = std::accumulate(sharpeInfo.dailyReturns.begin(), sharpeInfo.dailyReturns.end(), 0.0)/n;
  double rfDaily =  std::pow((1+sharpeInfo.riskFreeAnnual), 1.0/252) -1;
  
  sharpeInfo.variance = std::accumulate(sharpeInfo.dailyReturns.begin(), sharpeInfo.dailyReturns.end(), 0.0 , [=](double acc, double curr) {
    return acc + (curr-sharpeInfo.meanDailyReturns)*(curr-sharpeInfo.meanDailyReturns)/(n-1);
  });

  sharpeInfo.sharpe = (sharpeInfo.meanDailyReturns - rfDaily)/std::sqrt(sharpeInfo.variance) * std::sqrt(252);

  return true;
}

void Analytics::computeTradeClose() {

}
