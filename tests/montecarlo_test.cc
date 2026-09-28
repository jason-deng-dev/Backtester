#include "datafeed.h"
#include "montecarlo.h"
#include "state.h"
// must precede <gtest/gtest.h>: gtest pulls sys/wait.h, whose <stratSignal.h>
// resolves to THIS project's include/stratSignal.h (-I include). If it hasn't
// been pragma-onced yet, its templates land inside sys/wait.h's
// extern "C" block -> "template with C linkage". Renaming the project
// header is the real fix.
#include "stratSignal.h"
#include <gtest/gtest.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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
      "WWWWWWWWWWWWWW"        // d1 -d14: warmup
      "VVVVVVVVV"             // d15-d23: volatile (d15 labeled hot immediately)
      "CCCCCCCCCCCCCCCCCCCCC" // d24-d44: calm
      "VVVVVVVVVVVVVV"        // d45-d58: volatile
      "CCCCCCCCCCCC";         // d59-d70: calm
  ASSERT_EQ(expected.size(), feed.barCount());

  auto toRegime = [](char c) {
    return c == 'W'   ? Regime::WARMUP
           : c == 'V' ? Regime::VOLATILE
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

  MonteCarlo mc{};
  mc.classifyRegime(st, 0.75, 0.6, 5, 10);
  mc.createTransitionMatrix();

  EXPECT_DOUBLE_EQ(mc.getTransitionProb(RegimeSwitch::VolToVol),
                   21 / 23.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProb(RegimeSwitch::VolToCalm),
                   2 / 23.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProb(RegimeSwitch::CalmToCalm),
                   31 / 32.0);
  EXPECT_DOUBLE_EQ(mc.getTransitionProb(RegimeSwitch::CalmToVol),
                   1 / 32.0);
}

} // namespace classifyRegime

namespace sampling {

// ---------------------------------------------------------------------------
// Sampling fixture
//
// Bars come from tests/fixtures/regimeTest_bars.csv, whose labels are pinned by
// MonteCarloTest.classifyRegime. The regime runs are
//   d1-d14  WARMUP
//   d15-d23 VOLATILE
//   d24-d44 CALM
//   d45-d58 VOLATILE
//   d59-d70 CALM
//
// Four round trips are then closed on dates with known labels. entryNotional and
// exitNotional accumulate as direction * qty * price and pnl is
// exitNotional - entryNotional (Analytics::handleExecution), which gives each
// record a distinct fractional return. That distinctness is what lets a test map
// a sampled trade back to the record it was drawn from:
//
//   open  close  qty  openPx/closePx  entry  pnl   return      label
//   d20   d25     10  100/110          1000  +100    0.1       VOLATILE
//   d30   d40      5   50/45            250   -25   -0.1       CALM
//   d50   d52     20  200/210          4000  +200    0.05      VOLATILE
//   d60   d70     10  300/280          3000  -200   -200/3000  CALM
//
// So calmPositions = {d30, d60}, volPositions = {d20, d50}, every regime path is
// 4 trades long, and getRegimeProb(VOLATILE) is 23/56 -- 23 VOLATILE bars out of
// 56 labeled bars.
// ---------------------------------------------------------------------------

// -200/3000, spelled the same way the sampler computes it
constexpr double kLossRatio = -200.0 / 3000.0;

struct FixtureRecord {
  double fractionalReturn;
  Regime regime;
};

const std::array<FixtureRecord, 4> kFixtureRecords{{
    {0.1, Regime::VOLATILE},
    {-0.1, Regime::CALM},
    {0.05, Regime::VOLATILE},
    {kLossRatio, Regime::CALM},
}};

// index into kFixtureRecords, -1 when the return matches no record
int fixtureSlot(double r) {
  for (std::size_t i = 0; i < kFixtureRecords.size(); ++i) {
    if (kFixtureRecords[i].fractionalReturn == r)
      return static_cast<int>(i);
  }
  return -1;
}

struct Fixture {
  DataFeed feed;
  // netQty pinned at 1 so snapshot equity == close, as in the classifier tests
  State st{0, 1};
  Analytics an;

  bool build() {
    if (!feed.load(TEST_FIXTURES_DIR "/regimeTest_bars.csv"))
      return false;

    Bar bar{};
    while (feed.next(bar))
      st.addBarSnapshot(bar.date, bar.close, bar.low, bar.high);

    // executions are added after the snapshots, so they only feed Analytics and
    // leave the equity curve the classifier sees untouched
    const struct {
      const char *open, *close;
      int qty;
      double openPx, closePx;
    } trades[] = {
        {"d20", "d25", 10, 100.0, 110.0},
        {"d30", "d40", 5, 50.0, 45.0},
        {"d50", "d52", 20, 200.0, 210.0},
        {"d60", "d70", 10, 300.0, 280.0},
    };
    for (const auto &t : trades) {
      st.addExecution(t.open, t.qty, t.openPx);
      st.addExecution(t.close, -t.qty, t.closePx);
    }

    return an.captureState(st);
  }
};

// exact comparison on purpose: both sides run identical arithmetic
bool pathsEqual(const std::vector<std::vector<SampledTrade>> &a,
                const std::vector<std::vector<SampledTrade>> &b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (a[i].size() != b[i].size())
      return false;
    for (std::size_t j = 0; j < a[i].size(); ++j) {
      if (a[i][j].regime != b[i][j].regime ||
          a[i][j].fractionalReturn != b[i][j].fractionalReturn)
        return false;
    }
  }
  return true;
}

// classify with the lookbacks the classifier tests use, so the labels above hold
void classifyFixture(MonteCarlo &mc, const Fixture &f) {
  mc.classifyRegime(f.st, 0.75, 0.6, 5, 10);
}

void classifyBoth(MonteCarlo &a, MonteCarlo &b, const Fixture &f) {
  classifyFixture(a, f);
  classifyFixture(b, f);
}

// Pins the fixture itself. If this fails, the sampling tests below are reading a
// dataset other than the one their comments describe.
TEST(MonteCarloTest, samplingFixtureRecordsAndLabels) {
  Fixture f;
  ASSERT_TRUE(f.build());

  const auto &records = f.an.getPositionRecords();
  ASSERT_EQ(records.size(), kFixtureRecords.size());

  MonteCarlo mc{};
  classifyFixture(mc, f);

  for (std::size_t i = 0; i < records.size(); ++i) {
    const double ret = records[i].pnl / std::abs(records[i].entryNotional);
    EXPECT_DOUBLE_EQ(ret, kFixtureRecords[i].fractionalReturn) << "record " << i;
    EXPECT_EQ(mc.getRegime(records[i].openTime), kFixtureRecords[i].regime)
        << "record " << i;
  }

  int calm = 0;
  int vol = 0;
  for (const auto &r : records) {
    if (mc.getRegime(r.openTime) == Regime::CALM)
      ++calm;
    else
      ++vol;
  }
  EXPECT_EQ(calm, 2);
  EXPECT_EQ(vol, 2);
}

TEST(MonteCarloTest, sampleTradeRegimeRejectsEmptyBucket) {
  MonteCarlo mc{};
  const std::vector<PositionRecord> one(1);

  EXPECT_THROW(mc.sampleTradeRegime(0, 7, one, {}), std::logic_error);
  EXPECT_THROW(mc.sampleTradeRegime(0, 7, {}, one), std::logic_error);
  EXPECT_THROW(mc.sampleTradeRegime(0, 7, {}, {}), std::logic_error);
}

TEST(MonteCarloTest, samplingRejectsZeroPaths) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};

  EXPECT_THROW(mc.sampleTradesSerial(0, 1, f.an), std::logic_error);
  EXPECT_THROW(mc.sampleTradesParallel(0, 1, f.an), std::logic_error);
  EXPECT_THROW(mc.sampleTradesRegimeSerial(0, 1, f.an), std::logic_error);
  EXPECT_THROW(mc.sampleTradesRegimeParallel(0, 1, f.an), std::logic_error);
}

TEST(MonteCarloTest, getRegimeProbNeedsClassifiedData) {
  MonteCarlo mc{};
  EXPECT_THROW(mc.getRegimeData().getRegimeProb(Regime::VOLATILE),
               std::logic_error);
}

TEST(MonteCarloTest, getRegimeProbMatchesLabelCounts) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);

  const auto &rd = mc.getRegimeData();
  // 9 + 14 VOLATILE bars, 21 + 12 CALM bars, per the label runs
  EXPECT_DOUBLE_EQ(rd.getRegimeProb(Regime::VOLATILE), 23.0 / 56.0);
  EXPECT_DOUBLE_EQ(rd.getRegimeProb(Regime::CALM), 33.0 / 56.0);
  EXPECT_NEAR(rd.getRegimeProb(Regime::VOLATILE) +
                  rd.getRegimeProb(Regime::CALM),
              1.0, 1e-12);
  EXPECT_THROW(rd.getRegimeProb(Regime::WARMUP), std::invalid_argument);
}

// Every sampled trade must carry the regime of the bucket it was drawn from.
// The fixture's four returns are distinct, so the return identifies the record.
TEST(MonteCarloTest, sampleTradesPathInvariants) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);
  mc.sampleTradesSerial(8, 11, f.an);

  const auto &paths = mc.getSampledTrades();
  ASSERT_EQ(paths.size(), 8u);
  for (const auto &path : paths) {
    ASSERT_EQ(path.size(), f.an.getPositionRecords().size());
    for (const auto &t : path) {
      const int slot = fixtureSlot(t.fractionalReturn);
      ASSERT_GE(slot, 0) << t.fractionalReturn;
      EXPECT_NE(t.regime, Regime::WARMUP);
      // sampleTrade tags from the historical lookup at openTime
      EXPECT_EQ(t.regime, kFixtureRecords[slot].regime);
    }
  }
}

TEST(MonteCarloTest, sampleTradesRegimePathInvariants) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);
  mc.sampleTradesRegimeSerial(8, 11, f.an);

  const auto &paths = mc.getSampledTrades();
  ASSERT_EQ(paths.size(), 8u);
  for (const auto &path : paths) {
    // labeled positions only: the two CALM and the two VOLATILE records
    ASSERT_EQ(path.size(), 4u);
    for (const auto &t : path) {
      const int slot = fixtureSlot(t.fractionalReturn);
      ASSERT_GE(slot, 0) << t.fractionalReturn;
      EXPECT_NE(t.regime, Regime::WARMUP);
      // the tag must equal the simulated regime, which is the source bucket
      EXPECT_EQ(t.regime, kFixtureRecords[slot].regime);
    }
  }
}

TEST(MonteCarloTest, sameSeedReproducesPaths) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo a{};
  MonteCarlo b{};
  classifyBoth(a, b, f);

  a.sampleTradesSerial(16, 5, f.an);
  b.sampleTradesSerial(16, 5, f.an);
  EXPECT_TRUE(pathsEqual(a.getSampledTrades(), b.getSampledTrades()));

  a.sampleTradesRegimeSerial(16, 5, f.an);
  b.sampleTradesRegimeSerial(16, 5, f.an);
  EXPECT_TRUE(pathsEqual(a.getSampledTrades(), b.getSampledTrades()));
}

TEST(MonteCarloTest, differentSeedsDiverge) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo a{};
  MonteCarlo b{};
  classifyBoth(a, b, f);

  a.sampleTradesSerial(16, 5, f.an);
  b.sampleTradesSerial(16, 6, f.an);
  EXPECT_FALSE(pathsEqual(a.getSampledTrades(), b.getSampledTrades()));

  a.sampleTradesRegimeSerial(16, 5, f.an);
  b.sampleTradesRegimeSerial(16, 6, f.an);
  EXPECT_FALSE(pathsEqual(a.getSampledTrades(), b.getSampledTrades()));
}

// The parallel samplers partition path indices and seed each path with
// seed + i, so they must reproduce the serial result exactly.
TEST(MonteCarloTest, serialAndParallelAgree) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo a{};
  MonteCarlo b{};
  MonteCarlo c{};
  MonteCarlo d{};
  classifyBoth(a, b, f);
  classifyBoth(c, d, f);

  a.sampleTradesSerial(16, 5, f.an);
  b.sampleTradesParallel(16, 5, f.an);
  EXPECT_TRUE(pathsEqual(a.getSampledTrades(), b.getSampledTrades()));

  c.sampleTradesRegimeSerial(16, 5, f.an);
  d.sampleTradesRegimeParallel(16, 5, f.an);
  EXPECT_TRUE(pathsEqual(c.getSampledTrades(), d.getSampledTrades()));
}

// sampleTrade draws uniformly with replacement from all position records
TEST(MonteCarloTest, sampleTradesDrawsUniformly) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);
  constexpr int kPaths = 2000;
  mc.sampleTradesSerial(kPaths, 7, f.an);

  std::array<int, 4> hits{};
  int total = 0;
  for (const auto &path : mc.getSampledTrades()) {
    for (const auto &t : path) {
      const int slot = fixtureSlot(t.fractionalReturn);
      ASSERT_GE(slot, 0);
      ++hits[slot];
      ++total;
    }
  }

  ASSERT_EQ(total, kPaths * 4);
  const double expected = total / 4.0;
  // pinned seed, so this is deterministic; band is about 5 sd for total = 8000
  for (std::size_t i = 0; i < hits.size(); ++i)
    EXPECT_NEAR(hits[i], expected, 250.0) << "slot " << i;
}

// Path position 0 is drawn from bernoulli(volProb), so the share of paths that
// open in VOLATILE must approach getRegimeProb(VOLATILE).
TEST(MonteCarloTest, startRegimeFrequencyApproachesVolProb) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);
  constexpr int kPaths = 2000;
  mc.sampleTradesRegimeSerial(kPaths, 1234, f.an);

  int volStarts = 0;
  for (const auto &path : mc.getSampledTrades()) {
    ASSERT_FALSE(path.empty());
    if (path.front().regime == Regime::VOLATILE)
      ++volStarts;
  }

  const double expected = mc.getRegimeData().getRegimeProb(Regime::VOLATILE);
  // pinned seed keeps this deterministic; band is about 4.5 sd for 2000 paths
  EXPECT_NEAR(double(volStarts) / kPaths, expected, 0.035);
}

// The regime chain must behave like the transition matrix it is built from.
// This is the check that catches a distribution wired to the wrong switch, or a
// step that draws twice and lets the first draw be undone.
TEST(MonteCarloTest, regimeTransitionsMatchTransitionMatrix) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);
  constexpr int kPaths = 2000;
  mc.sampleTradesRegimeSerial(kPaths, 99, f.an);

  // RegimeSwitch order: CalmToCalm, CalmToVol, VolToCalm, VolToVol
  std::array<int, 4> counts{};
  int fromCalm = 0;
  int fromVol = 0;

  for (const auto &path : mc.getSampledTrades()) {
    for (std::size_t j = 1; j < path.size(); ++j) {
      const Regime prev = path[j - 1].regime;
      const Regime curr = path[j].regime;
      if (prev == Regime::CALM) {
        ++fromCalm;
        ++counts[curr == Regime::CALM ? 0 : 1];
      } else {
        ++fromVol;
        ++counts[curr == Regime::VOLATILE ? 3 : 2];
      }
    }
  }

  ASSERT_GT(fromCalm, 0);
  ASSERT_GT(fromVol, 0);
  for (std::size_t s = 0; s < counts.size(); ++s) {
    const int from = s <= 1 ? fromCalm : fromVol;
    EXPECT_NEAR(double(counts[s]) / from,
                mc.getTransitionProb(static_cast<RegimeSwitch>(s)), 0.02)
        << "switch " << s;
  }
}

} // namespace sampling

namespace pathStats {

using sampling::Fixture;
using sampling::classifyFixture;

TEST(MonteCarloTest, computeAllPathStatRequiresSampledTrades) {
  MonteCarlo mc{};
  EXPECT_THROW(mc.computeAllPathStatSerial(1000.0), std::logic_error);
}

TEST(MonteCarloTest, computeAllPathStatReplaysEachPath) {
  Fixture f;
  ASSERT_TRUE(f.build());
  MonteCarlo mc{};
  classifyFixture(mc, f);

  constexpr int kPaths = 32;
  constexpr double kStart = 10000.0;
  mc.sampleTradesRegimeSerial(kPaths, 3, f.an);
  mc.computeAllPathStatSerial(kStart);

  const auto &paths = mc.getSampledTrades();
  const auto &out = mc.getOutcomes();
  ASSERT_EQ(out.size(), paths.size());

  for (std::size_t i = 0; i < paths.size(); ++i) {
    ASSERT_FALSE(paths[i].empty()) << i;

    double balance = kStart;
    double product = kStart; // independent formulation of the same quantity
    double peak = kStart;
    double trough = kStart;
    double maxDrawDown = 0.0;
    for (const auto &t : paths[i]) {
      balance += balance * t.fractionalReturn;
      product *= 1.0 + t.fractionalReturn;
      peak = std::max(peak, balance);
      trough = std::min(trough, balance);
      maxDrawDown = std::max(maxDrawDown, (peak - balance) / peak);
    }

    EXPECT_DOUBLE_EQ(out[i].balance, balance) << i;
    EXPECT_NEAR(out[i].balance, product, 1e-9) << i;
    EXPECT_DOUBLE_EQ(out[i].peak, peak) << i;
    EXPECT_DOUBLE_EQ(out[i].trough, trough) << i;
    EXPECT_DOUBLE_EQ(out[i].maxDrawdown, maxDrawDown) << i;

    // invariants that hold regardless of the drawn path
    EXPECT_GE(out[i].peak, kStart) << i;
    EXPECT_LE(out[i].trough, kStart) << i;
    EXPECT_GE(out[i].maxDrawdown, 0.0) << i;
    EXPECT_LT(out[i].maxDrawdown, 1.0) << i;
    EXPECT_LE(out[i].trough, out[i].balance) << i;
    EXPECT_GE(out[i].peak, out[i].balance) << i;
  }
}

} // namespace pathStats
