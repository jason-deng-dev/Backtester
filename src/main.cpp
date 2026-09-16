#include "backtest.h"
#include "datafeed.h"
#include "strategy.h"
#include "sizer.h"
#include "signal.h"
#include "riskmanager.h"
#include "state.h"
#include "analytics.h"


int main() {
  

  DataFeed df{};
  df.load("data/nvda_daily.csv");

  BuyHoldSignal signal;
  FixedFractionalSizer sizer{1};
  NotionalCapRiskManager riskManager{0.1, NotionalCapRiskManager::Policy::Clamp};

  Strategy rs{signal, sizer, riskManager};

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
