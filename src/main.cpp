#include "backtest.h"
#include "datafeed.h"
#include "strategy.h"
#include "state.h"
#include "analytics.h"


int main() {
  

  DataFeed df{};
  df.load("data/nvda_daily.csv");

  naive_reversion_strategy rs{};

  State st(100000, 0);

  Backtest bt{};
  bt.run(df, rs, st);

  Analytics an{};

  an.captureState(st);
  an.report();
  // an.reportPositions();
  // an.reportExits();

  return 0;
}
