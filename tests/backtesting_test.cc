#include "analytics.h"
#include "datafeed.h"
#include "riskmanager.h"
#include "signal.h"
#include "sizer.h"
#include "state.h"
#include "strategy.h"
#include <gtest/gtest.h>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

/*
  Precondition: tests/fixtures/aapl_daily.csv via yfinance,
  interval = 1d, period = max, auto_adjust = true
  current file has data from 1980-12-12 (row 4) to 2026-09-09 (row 11530)
*/

namespace DataFeedTest {
TEST(DataFeedTest, Load) {
  DataFeed df;
  EXPECT_FALSE(df.load("wrong file path"));
  EXPECT_TRUE(df.load(TEST_FIXTURES_DIR "/aapl_daily.csv"));
  EXPECT_EQ(df.barCount(), 11527);
}

TEST(DataFeedTest, Next) {
  DataFeed df;
  df.load(TEST_FIXTURES_DIR "/aapl_daily.csv");
  Bar bar{};
  df.next(bar);
  Bar expectBar1{"1980-12-12",        0.09812235087156296, 0.09854894611290255,
                 0.09812235087156296, 0.09812235087156296, 469033600};
  EXPECT_EQ(bar, expectBar1);
  df.next(bar);
  Bar expectBar2{"1980-12-15",       0.0930032730102539,  0.09342986836701266,
                 0.0930032730102539, 0.09342986836701266, 175884800};
  EXPECT_EQ(bar, expectBar2);
  df.next(bar);
  Bar expectBar3{"1980-12-16",        0.08617705851793289, 0.08660364832181397,
                 0.08617705851793289, 0.08660364832181397, 105728000};
  EXPECT_EQ(bar, expectBar3);
}

} // namespace DataFeedTest

namespace StateTest {
TEST(StateTest, Init) {
  State st1{};
  EXPECT_TRUE(st1.getCash() == 0 && st1.getNetQty() == 0 &&
              st1.getBarSnapshots().size() == 1 &&
              st1.getExecutions().size() == 0);

  State st2{1'000'000, 10};
  EXPECT_TRUE(st2.getCash() == 1000000 && st2.getNetQty() == 10 &&
              st2.getBarSnapshots().size() == 1 &&
              st2.getExecutions().size() == 0);
}

TEST(StateTest, addBarSnapshot) {
  State st{10'000, 0};
  ASSERT_EQ(st.getBarSnapshots().size(), 1); // "before trading" sentinel

  st.addBarSnapshot("1980-12-12", 100.0, 95.0, 105.0);
  ASSERT_EQ(st.getBarSnapshots().size(), 2);
  const BarSnapshot &snap = st.getBarSnapshots().back();
  EXPECT_EQ(snap.date, "1980-12-12");
  EXPECT_EQ(snap.netQty, 0);
  EXPECT_DOUBLE_EQ(snap.equity, 10000.0); // flat: equity is just cash
  EXPECT_DOUBLE_EQ(snap.minPrice, 95.0);
  EXPECT_DOUBLE_EQ(snap.maxPrice, 105.0);

  // position marked to market: +10/share on 10 shares shows up in equity
  st.addExecution("1980-12-12", 10, 100.0);
  st.addBarSnapshot("1980-12-15", 110.0, 100.0, 115.0);
  EXPECT_DOUBLE_EQ(st.getBarSnapshots().back().equity, 10100.0);
}

TEST(StateTest, addExecution) {
  State st{10'000, 0};

  st.addExecution("1980-12-12", 10, 100.0);
  EXPECT_EQ(st.getCash(), 9000);
  EXPECT_EQ(st.getNetQty(), 10);
  ASSERT_EQ(st.getExecutions().size(), 1);
  EXPECT_EQ(st.getExecutions()[0].date, "1980-12-12");
  EXPECT_EQ(st.getExecutions()[0].qty, 10);
  EXPECT_DOUBLE_EQ(st.getExecutions()[0].price, 100.0);

  // sell flips both signs
  st.addExecution("1980-12-15", -4, 110.0);
  EXPECT_EQ(st.getCash(), 9440);
  EXPECT_EQ(st.getNetQty(), 6);

  // a self-financing trade at price p leaves equity valued at p unchanged
  double equityBefore = st.getEquity(120.0);
  st.addExecution("1980-12-16", 2, 120.0);
  EXPECT_DOUBLE_EQ(st.getEquity(120.0), equityBefore);
}

TEST(StateTest, LongAvgEntryPrice) {
  State st{10'000, 0};
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 0);
  st.addExecution("d1", 100, 10);
  // 100*10/10 = 100
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0)
      << "adding long position from 0";
  st.addExecution("d2", -10, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0) << "reducing long position";
  st.addExecution("d3", 50, 20);
  // d2 leaves 90 shares @ 10: (10*90 + 20*50)/(90+50)
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 1900 / 140.0)
      << "adding long position after reduction";
  EXPECT_EQ(st.getNetQty(), 140);
  st.addExecution("d4", -150, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10) << "reverse long";
}

TEST(StateTest, ShortAvgEntryPrice) {
  State st{10'000, 0};
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 0);
  st.addExecution("d1", -100, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0)
      << "adding short position from 0";
  st.addExecution("d2", 10, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0) << "reducing short position";
  st.addExecution("d3", -50, 20);
  // d2 leaves 90 shares @ 10: (10*90 + 20*50)/(90+50)
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 1900 / 140.0)
      << "adding short position after reduction";

  EXPECT_EQ(st.getNetQty(), -140);
  st.addExecution("d4", 150, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10) << "reverse short";
}

TEST(StateTest, AvgEntryPriceFlatResetLong) {
  State st{10'000, 0};
  st.addExecution("d1", 100, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0);
  st.addExecution("d2", -100, 15);
  ASSERT_EQ(st.getNetQty(), 0) << "position fully closed";

  // reopening starts a fresh episode, it must not reuse the old basis
  st.addExecution("d3", 50, 30);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 30.0) << "reopen long after flat";

  st.addExecution("d4", -50, 40);
  ASSERT_EQ(st.getNetQty(), 0);
  st.addExecution("d5", -75, 8);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 8.0) << "open short after flat";
}

TEST(StateTest, AvgEntryPriceFlatResetShort) {
  State st{10'000, 0};
  st.addExecution("d1", -100, 10);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 10.0);
  st.addExecution("d2", 100, 12);
  ASSERT_EQ(st.getNetQty(), 0) << "short fully covered";

  // with a stale entryQty of -100 here, the old formula divides by zero
  st.addExecution("d3", 100, 30);
  EXPECT_DOUBLE_EQ(st.getAvgEntryPrice(), 30.0) << "reopen long after short";
}

} // namespace StateTest

namespace StrategyTest {

namespace SignalTest {
TEST(SignalTest, RandomSignal) {
  std::mt19937 gen{42};
  EXPECT_THROW(RandomSignal(2, 1, 1, gen), std::invalid_argument);
  EXPECT_THROW(RandomSignal(1, 2, 1, gen), std::invalid_argument);
  EXPECT_THROW(RandomSignal(1, 1, 2, gen), std::invalid_argument);

  // below-range violations, and 0/1 are the valid inclusive boundaries
  EXPECT_THROW(RandomSignal(-0.1, 0.5, 0.5, gen), std::invalid_argument);
  EXPECT_THROW(RandomSignal(0.5, -0.1, 0.5, gen), std::invalid_argument);
  EXPECT_THROW(RandomSignal(0.5, 0.5, -0.1, gen), std::invalid_argument);
  EXPECT_NO_THROW(RandomSignal(0, 0, 0, gen));
  EXPECT_NO_THROW(RandomSignal(1, 1, 1, gen));
}

// boundary probabilities make every draw deterministic: bernoulli(1) always
// fires, bernoulli(0) never does, so no seed dependence anywhere below

TEST(SignalTest, RandomSignalEnterLong) {
  std::mt19937 gen{42};
  RandomSignal rs(1, 0, 1, gen); // always enter, always long
  State st{100'000, 0};
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), 1) << "flat + always-enter-long";
}

TEST(SignalTest, RandomSignalEnterShort) {
  std::mt19937 gen{42};
  RandomSignal rs(1, 0, 0, gen); // always enter, always short
  State st{100'000, 0};
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), -1) << "flat + always-enter-short";
}

TEST(SignalTest, RandomSignalNeverEnter) {
  std::mt19937 gen{42};
  RandomSignal rs(0, 1, 0.5, gen); // pEnter = 0
  State st{100'000, 0};
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), 0) << "flat + never enter: silent";
}

TEST(SignalTest, RandomSignalExitLong) {
  std::mt19937 gen{42};
  RandomSignal rs(0, 1, 0.5, gen); // always exit when in position
  State st{100'000, 100};          // long
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), -1) << "long position exits with a sell";
}

TEST(SignalTest, RandomSignalExitShort) {
  std::mt19937 gen{42};
  RandomSignal rs(0, 1, 0.5, gen);
  State st{100'000, -100}; // short
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), 1) << "short position exits with a buy";
}

TEST(SignalTest, RandomSignalHoldPosition) {
  std::mt19937 gen{42};
  RandomSignal rs(1, 0, 0.5, gen); // pExit = 0
  State st{100'000, 100};
  std::vector<Bar> hs{};
  for (int i = 0; i < 10; i++)
    EXPECT_EQ(rs.generate(st, hs), 0)
        << "in position: no exits, and no mid-position entries";
}

} // namespace SignalTest

namespace RiskManagerTest {

TEST(RiskManagerTest, LongNotionalCapRiskManager) {
  NotionalCapRiskManager ncr{0.1};
  State st{100, 10};
  std::vector<Bar> hs{};
  /*
  Cases:
    Long/Short
      overcap
      oncap
      undercap
      reducing
      flipping sign
  */
  EXPECT_EQ(ncr.generate(1, st, hs), 0) << "empty history check";
  hs.push_back({"A"});
  EXPECT_EQ(ncr.generate(0, st, hs), 0) << "0 sizerOutput";
  hs.pop_back();
  hs.push_back({"Date", 1});
  // equity = 100 + 10*1 = 110
  // capNotional = 110*0.1 = 11
  // allowedAmount = capNotional / price - netQty = 11/1.0 = 11.0 - 10 = 1
  EXPECT_EQ(ncr.generate(2, st, hs), 1) << "overcap";
  EXPECT_EQ(ncr.generate(1, st, hs), 1) << "oncap";
  EXPECT_EQ(ncr.generate(0.5, st, hs), 0.5) << "undercap";
  EXPECT_EQ(ncr.generate(-2, st, hs), -2) << "reduce";
  EXPECT_EQ(ncr.generate(-11, st, hs), -11) << "reversalUndercap";
  EXPECT_EQ(ncr.generate(-22, st, hs), -21) << "reversalOvercap";
}

TEST(RiskManagerTest, ShortNotionalCapRiskmanager) {
  NotionalCapRiskManager ncr{0.1};
  State st{100, -5};
  std::vector<Bar> hs{};
  hs.push_back({"Date", 1});
  // equity = 100 + (-5)*1 = 95
  // capNotional = 95*0.1 = 9.5 -> capQty = 9.5 shares
  EXPECT_EQ(ncr.generate(-2, st, hs), -2) << "undercap increase short";
  EXPECT_EQ(ncr.generate(-4.5, st, hs), -4.5) << "oncap short";
  EXPECT_EQ(ncr.generate(-6, st, hs), -4.5)
      << "overcap increase short clamps to cap";
  EXPECT_EQ(ncr.generate(2, st, hs), 2) << "reduce short";
  EXPECT_EQ(ncr.generate(5, st, hs), 5) << "close out to flat";
  EXPECT_EQ(ncr.generate(8, st, hs), 8) << "reversal undercap";
  EXPECT_EQ(ncr.generate(20, st, hs), 14.5)
      << "reversal overcap: close 5, establish 9.5";

  // already over cap: risk-reducing orders must pass untouched
  State st2{100, -12}; // equity = 88 -> capQty = 8.8
  EXPECT_EQ(ncr.generate(2, st2, hs), 2) << "reduce while overcap";
}

TEST(RiskManagerTest, TrueRanges) {
  TrueRanges tr{5};
  tr.addTR(1);
  EXPECT_EQ(tr.getATR(), std::nullopt) << "1/5";
  tr.addTR(1);
  tr.addTR(1);
  tr.addTR(1);
  EXPECT_EQ(tr.getATR(), std::nullopt) << "4/5";
  tr.addTR(1);
  EXPECT_DOUBLE_EQ(*tr.getATR(), 1.0);

  tr.addTR(2);
  // Wilder pins the divisor at the period: (1*4 + 2)/5
  EXPECT_DOUBLE_EQ(*tr.getATR(), 6 / 5.0);

  tr.addTR(3);
  // (6/5*4 + 3)/5
  EXPECT_DOUBLE_EQ(*tr.getATR(), 39 / 25.0);
}

TEST(RiskManagerTest, BracketRiskManagerWarmup) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", 100, 100.0); // long 100 @ 100

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0}; // TR = 2 vs a 100 prevClose

  hs.push_back(calm);
  EXPECT_EQ(brm.generate(5, st, hs), 5) << "history < 2 bars: passthrough";

  hs.push_back(calm);
  hs.back().close = 97; // below the future stop of 98
  EXPECT_EQ(brm.generate(5, st, hs), 5) << "ATR not seeded yet: passthrough";
}

TEST(RiskManagerTest, BracketRiskManagerFlatPassthrough) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0}; // no position: nothing to bracket

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(25, st, hs);
  hs.push_back(calm);
  brm.generate(25, st, hs);
  hs.push_back(calm);
  hs.back().close = 97; // beyond any level, but flat means no trigger
  EXPECT_EQ(brm.generate(25, st, hs), 25) << "flat: sizer passes through";
}

TEST(RiskManagerTest, BracketRiskManagerLongStop) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", 100, 100.0); // long 100 @ 100

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  EXPECT_EQ(brm.generate(5, st, hs), 5) << "armed, close=100 touches nothing";

  hs.back().close = 97; // close < stop 98
  EXPECT_EQ(brm.generate(5, st, hs), -100) << "stop fires: full exit";
  EXPECT_EQ(brm.generate(50, st, hs), -100)
      << "trigger precedence: sizer's add is discarded";
}

TEST(RiskManagerTest, BracketRiskManagerLongTake) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", 100, 100.0);

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);

  hs.back().close = 107; // close > take 106
  EXPECT_EQ(brm.generate(5, st, hs), -100) << "take-profit fires: full exit";
}

TEST(RiskManagerTest, BracketRiskManagerShortStop) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", -100, 100.0); // short 100 @ 100

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);

  hs.back().close = 103; // close > stop 102 (shorts stop out on the way up)
  EXPECT_EQ(brm.generate(5, st, hs), 100) << "short stop fires: cover to flat";
}

TEST(RiskManagerTest, BracketRiskManagerShortTake) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", -100, 100.0);

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);

  hs.back().close = 93; // close < take 94 (shorts take profit on the way down)
  EXPECT_EQ(brm.generate(5, st, hs), 100) << "short take fires: cover to flat";
}

TEST(RiskManagerTest, BracketRiskManagerNoTouch) {
  BracketRiskManager brm{2, 1.0, 3.0};
  State st{100'000, 0};
  st.addExecution("d0", 100, 100.0); // long 100 @ 100, stop 98 / take 106

  std::vector<Bar> hs{};
  Bar calm{"d", 100, 101, 99, 100, 0};
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);
  hs.push_back(calm);
  brm.generate(5, st, hs);

  hs.back().close = 98; // exactly on the stop: strict < means no trigger
  EXPECT_EQ(brm.generate(5, st, hs), 5) << "touching the level is not crossing";
  EXPECT_EQ(brm.generate(-40, st, hs), -40)
      << "normal reductions pass through untouched";
}

} // namespace RiskManagerTest

} // namespace StrategyTest

// Analytics tests
TEST(AnalyticsTest, Classify) {
  EXPECT_EQ(classify(0), Outcome::BreakEven);
  EXPECT_EQ(classify(1), Outcome::Win);
  EXPECT_EQ(classify(-1), Outcome::Loss);
}
