#include "analytics.h"
#include <algorithm>
#include <cstdlib>
#include <deque>

void Analytics::captureState(const State &state) {
  // BarSnapshot<date, equity, netQty, minPrice, maxPrice>
  const auto &barSnapshots = state.getBarSnapshots();
  // Execution<date, qty, price>
  const auto &executions = state.getExecutions();

  recordInfo(executions, barSnapshots);
}

auto sign = [](int q) { return (q > 0) - (q < 0); };

/*
  Execution <date, qty, price>
  qty > 0 = buy, qty < 0 = sell

  OpenPosition <date, direction, qty, price>
  direction == 1 = long position
  direction == -1 = short position
*/

void Analytics::recordExecutions(const std::vector<Execution> &executions) {
  std::deque<OpenPosition> openPositions;
  std::string openTime{};
  double entryNotional{0};
  double exitNotional{0};

  for (auto [date, qty, price] : executions) {
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
                                 exitNotional, exitNotional - entryNotional});
      entryNotional = 0;
      exitNotional = 0;
    }

    // remainder opens a new position or extends the current one
    if (remaining != 0) {
      if (openPositions.empty()) {
        openTime = date;
      }
      openPositions.push_back({date, sign(qty), remaining, price});
      entryNotional += sign(qty) * remaining * price;
    }
  }
}






void Analytics::recordInfo(const std::vector<Execution> &executions, const std::vector<BarSnapshot> &barSnapshots) {
  int executionIndex = 0;

  for (auto& )





}
