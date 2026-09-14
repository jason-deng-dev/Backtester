#include "backtest.h"
#include "datafeed.h"
#include "state.h"
#include "strategy.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>

void Backtest::run(DataFeed &feed, Strategy &strategy, State &state) {

  Bar bar{};
  double maxPrice = -std::numeric_limits<double>::infinity();
  double minPrice = std::numeric_limits<double>::infinity();

  while (feed.next(bar)) {
    int qty = strategy.getMove(state, bar.open, bar.close);

    int qtyBefore = state.getNetQty();



    // fill first
    bool increasingPosition =
        std::abs(state.getNetQty() + qty) > std::abs(state.getNetQty());
    bool canAffordPosition = std::abs(qty) * bar.open <= state.getCash();
    if (qty != 0 && (!increasingPosition || canAffordPosition)) {
      state.addExecution(bar.date, qty, bar.open);
    }
    state.addEquity(bar.date, bar.close, minPrice, maxPrice);

    int qtyAfter = state.getNetQty();

    // reconcile window against new position
    if (qtyAfter == 0) {
      maxPrice = -std::numeric_limits<double>::infinity();
      minPrice = std::numeric_limits<double>::infinity();
    } else if (qtyBefore == 0 || (qtyBefore > 0) != (qtyAfter > 0)) {
      // episode starts here (from flat or flip)
      maxPrice = bar.high;
      minPrice = bar.low;
    } else {
      // episode continues — extend
      maxPrice = std::max(maxPrice, bar.high);
      minPrice = std::min(minPrice, bar.low);
    }
  }
  std::cout << "Backtest complete\n";
}
