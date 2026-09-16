#include "analytics.h"
#include "datafeed.h"
#include "state.h"
#include <gtest/gtest.h>

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

} // namespace StateTest



// Strategy tests

// Analytics tests
TEST(AnalyticsTest, Classify) {
  EXPECT_EQ(classify(0), Outcome::BreakEven);
  EXPECT_EQ(classify(1), Outcome::Win);
  EXPECT_EQ(classify(-1), Outcome::Loss);
}
