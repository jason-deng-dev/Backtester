#include "datafeed.h"
#include "montecarlo.h"
#include "state.h"
// must precede <gtest/gtest.h>: gtest pulls sys/wait.h, whose <signal.h>
// resolves to THIS project's include/signal.h (-I include). If it hasn't
// been pragma-onced yet, its templates land inside sys/wait.h's
// extern "C" block -> "template with C linkage". Renaming the project
// header is the real fix.
#include "signal.h"
#include <gtest/gtest.h>
#include <string>

namespace classifyRegime {

// Fixture: tests/fixtures/regimeTest_bars.csv — 70 synthetic bars with known
// regime structure (see scripts/gen_regime_fixture.py, which also computes
// the expected labels with an independent reimplementation of the
// classifier):
//   d1 -d14  calm      -> WARMUP (needs returnLookback + volLookback bars)
//   d15-d24  volatile  -> d15 is the FIRST labeled bar: it must be labeled
//                         VOLATILE immediately (first real bar overrides the
//                         initial CALM default; only transition COUNTING
//                         requires a predecessor)
//   d25-d44  calm      -> V->C flip, long calm run
//   d45-d59  volatile  -> C->V flip
//   d60-d70  calm      -> V->C flip, ends CALM
TEST(MonteCarloTest, classifyRegime) {
  DataFeed feed;
  ASSERT_TRUE(feed.load(TEST_FIXTURES_DIR "/regimeTest_bars.csv"));

  // netQty pinned at 1 -> snapshot equity == close, so equity returns are
  // the market's close-to-close returns
  State st{0, 1};
  Bar bar{};
  while (feed.next(bar)) {
    st.addBarSnapshot(bar.date, bar.close, bar.low, bar.high);
  }

  MonteCarlo mc{};
  mc.classifyRegime(st, 0.75, 0.6, 5, 10);

  // W = WARMUP, V = VOLATILE, C = CALM; expected[i] is the label for bar d(i+1)
  const std::string expected =
      "WWWWWWWWWWWWWW" // d1 -d14: warmup
      "VVVVVVVVV"      // d15-d23: volatile (d15 labeled hot immediately)
      "CCCCCCCCCCCCCCCCCCCCC" // d24-d44: calm
      "VVVVVVVVVVVVVV"        // d45-d58: volatile
      "CCCCCCCCCCCC";         // d59-d70: calm
  ASSERT_EQ(expected.size(), feed.barCount());

  auto toRegime = [](char c) {
    return c == 'W' ? Regime::WARMUP : c == 'V' ? Regime::VOLATILE
                                                : Regime::CALM;
  };
  for (std::size_t i = 0; i < expected.size(); ++i) {
    const std::string date = "d" + std::to_string(i + 1);
    EXPECT_EQ(mc.getRegime(date), toRegime(expected[i])) << date;
  }
}

// pins the warmup-length math explicitly: the first non-WARMUP label must
// land on bar d(returnLookback + volLookback) = d15
TEST(MonteCarloTest, classifyRegimeWarmupBoundary) {
  DataFeed feed;
  ASSERT_TRUE(feed.load(TEST_FIXTURES_DIR "/regimeTest_bars.csv"));

  State st{0, 1};
  Bar bar{};
  while (feed.next(bar)) {
    st.addBarSnapshot(bar.date, bar.close, bar.low, bar.high);
  }

  MonteCarlo mc{};
  mc.classifyRegime(st, 0.75, 0.6, 5, 10);

  EXPECT_EQ(mc.getRegime("d14"), Regime::WARMUP);
  EXPECT_EQ(mc.getRegime("d15"), Regime::VOLATILE);
}

} // namespace classifyRegime

TEST(MonteCarloTest, createTransitionMatrix) {
  // 9 V's => 21 C's => 14 V's => 12 C's
  // V to V: 21 => 21/23 =
  // V to C: 2  => 2/23
  // C to C: 31 => 31/32
  // C to V: 1  => 1/32
  DataFeed feed;
  feed.load(TEST_FIXTURES_DIR "/regimeTest_bars.csv");
  State st{0, 1};
  Bar bar{};
  while (feed.next(bar)) {
    st.addBarSnapshot(bar.date, bar.close, bar.low, bar.high);
  }

  MonteCarlo mc {};
  mc.classifyRegime(st, 0.75, 0.6, 5, 10);
  mc.createTransitionMatrix();

  EXPECT_DOUBLE_EQ(mc.getTransitionProbability(RegimeSwitch::VolToVol), 21/23.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProbability(RegimeSwitch::VolToCalm), 2/23.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProbability(RegimeSwitch::CalmToCalm), 31/32.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProbability(RegimeSwitch::CalmToVol), 1/32.0);
}
