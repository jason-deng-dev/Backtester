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
    state.addEquity(bar.date, bar.close, bar.low, bar.high);

    int qtyAfter = state.getNetQty();
  }
  std::cout << "Backtest complete\n";
}
