#include "analytics.h"
#include "state.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <optional>
#include <sstream>

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
      const double favorable = netQty * ((netQty > 0 ? maxPrice : minPrice) -
                                         episodeState.avgEntryPrice);
      const double adverse = netQty * ((netQty > 0 ? minPrice : maxPrice) -
                                       episodeState.avgEntryPrice);

      episodeState.mfe = std::max(favorable, episodeState.mfe);
      episodeState.mae = std::min(adverse, episodeState.mae);
    }
  }
}

auto sign = [](int q) { return (q > 0) - (q < 0); };

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
  if (n < 2)
    return false;
  sharpeInfo.meanDailyReturns =
      std::accumulate(sharpeInfo.dailyReturns.begin(),
                      sharpeInfo.dailyReturns.end(), 0.0) /
      n;
  double rfDaily = std::pow((1 + sharpeInfo.riskFreeAnnual), 1.0 / 252) - 1;

  sharpeInfo.variance = std::accumulate(
      sharpeInfo.dailyReturns.begin(), sharpeInfo.dailyReturns.end(), 0.0,
      [=](double acc, double curr) {
        return acc + (curr - sharpeInfo.meanDailyReturns) *
                         (curr - sharpeInfo.meanDailyReturns) / (n - 1);
      });

  // flat equity curve leaves std == 0, which would make sharpe inf/nan
  if (sharpeInfo.variance <= 0)
    return false;

  sharpeInfo.sharpe = (sharpeInfo.meanDailyReturns - rfDaily) /
                      std::sqrt(sharpeInfo.variance) * std::sqrt(252);

  return true;
}

void Analytics::computePositionRecords() {
  // recompute from scratch so calling this twice does not double count
  excursions.clear();
  positionInfo = TradeStats{};
  positionInfo.num = positionRecords.size();

  // for each position, ++numWinningPositions, +=
  for (const auto &pr : positionRecords) {
    const Outcome outcome = classify(pr.pnl);
    excursions.push_back({pr.mae / std::abs(pr.entryNotional),
                          pr.mfe / std::abs(pr.entryNotional),
                          pr.direction > 0, outcome});

    switch (outcome) {
    case Outcome::Win:
      ++positionInfo.numWin;
      positionInfo.grossProfit += pr.pnl;
      break;
    case Outcome::Loss:
      positionInfo.grossLoss += pr.pnl;
      break;
    case Outcome::BreakEven:
      ++positionInfo.numBreakEven;
      break;
    }
  }
}

void Analytics::computeExitRecords() {
  // recompute from scratch so calling this twice does not double count
  exitsInfo = TradeStats{};
  exitsInfo.num = exitRecords.size();
  for (const auto &ex : exitRecords) {
    switch (classify(ex.pnl)) {
    case Outcome::Win:
      ++exitsInfo.numWin;
      exitsInfo.grossProfit += ex.pnl;
      break;
    case Outcome::Loss:
      exitsInfo.grossLoss += ex.pnl;
      break;
    case Outcome::BreakEven:
      ++exitsInfo.numBreakEven;
      break;
    }
  }
}

namespace {

void line(std::ostream &os, const std::string &label, const std::string &value) {
  os << "  " << std::left << std::setw(22) << label << value << '\n';
}

// 1234567.5 -> "$1,234,567.50"
std::string money(double v) {
  std::ostringstream raw;
  raw << std::fixed << std::setprecision(2) << std::abs(v);
  std::string s = raw.str();
  for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(s.find('.')) - 3; i > 0;
       i -= 3) {
    s.insert(i, ",");
  }
  return (v < 0 ? "-$" : "$") + s;
}

// v is a fraction (0.05 -> "5.00%")
std::string pct(double v, bool sign = false) {
  std::ostringstream os;
  if (sign)
    os << std::showpos;
  os << std::fixed << std::setprecision(2) << v * 100 << "%";
  return os.str();
}

std::string num(double v) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << v;
  return os.str();
}

std::string pctOrNa(const std::optional<double> &v, bool sign = false) {
  return v ? pct(*v, sign) : "n/a";
}

std::optional<double> negate(const std::optional<double> &v) {
  return v ? std::optional<double>{-*v} : std::nullopt;
}

// nullopt means "don't filter on that axis"
std::vector<Excursion> filter(const std::vector<Excursion> &excursions,
                              std::optional<bool> isLong,
                              std::optional<Outcome> outcome) {
  std::vector<Excursion> v;
  std::copy_if(excursions.begin(), excursions.end(), std::back_inserter(v),
               [&](const Excursion &ex) {
                 return (!isLong || ex.isLong == *isLong) &&
                        (!outcome || ex.outcome == *outcome);
               });
  return v;
}

// precondition: v is sorted ascending
std::optional<double> median(const std::vector<double> &v) {
  if (v.empty())
    return std::nullopt;
  const std::size_t n = v.size();
  return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// upper tail quantile: p fraction of the sample is <= the result
// precondition: v is sorted ascending
// enforce minNeeded so we have statistically valuable percentile result
std::optional<double> percentile(const std::vector<double> &v, double p) {
  if (v.empty() || p < 0 || p > 1)
    return std::nullopt;
  if (p >= 1.0)
    return v.back();

  const std::size_t minNeeded = std::ceil(1.0 / (1.0 - p)) * 2;
  const std::size_t n = v.size();
  if (n < minNeeded)
    return std::nullopt;

  const std::size_t k = std::clamp<std::size_t>(std::ceil(p * n), 1, n);
  return v[k - 1];
}

struct ExcursionSummary {
  std::size_t n{};
  std::optional<double> maeMedian, maeP90, maeP95;
  std::optional<double> mfeMedian, mfeP75, mfeP90;
};

ExcursionSummary summarize(const std::vector<Excursion> &v) {
  ExcursionSummary s{};
  s.n = v.size();
  if (v.empty())
    return s;

  std::vector<double> mae, mfe;
  mae.reserve(v.size());
  mfe.reserve(v.size());
  for (const auto &e : v) {
    mae.push_back(e.maeNorm);
    mfe.push_back(e.mfeNorm);
  }
  std::sort(mae.begin(), mae.end());
  std::sort(mfe.begin(), mfe.end());

  s.maeMedian = median(mae);
  s.mfeMedian = median(mfe);

  // mae is signed and negative when adverse. the percentile is quoted as the
  // adverse magnitude ("90% of trades drew down no worse than 2.1%"), so flip
  // into ascending magnitude order and flip the result back.
  std::vector<double> adverse(mae.size());
  for (std::size_t i = 0; i < mae.size(); ++i) {
    adverse[i] = -mae[mae.size() - 1 - i];
  }

  s.maeP90 = negate(percentile(adverse, 0.90));
  s.maeP95 = negate(percentile(adverse, 0.95));
  s.mfeP75 = percentile(mfe, 0.75);
  s.mfeP90 = percentile(mfe, 0.90);
  return s;
}

} // namespace

void Analytics::report(std::ostream &os) {
  const bool sharpeOk = computeSharpe();
  computePositionRecords();
  computeExitRecords();

  os << "==================== BACKTEST REPORT ====================\n";
  reportCurve(os, sharpeOk);
  reportTradeStats(os, "Per-Position (round trip)", positionInfo);
  reportTradeStats(os, "Per-Exit (matched lot)", exitsInfo);

  os << "\n  Per-position net pnl counts closed round trips only. A position still\n"
        "  open on the last bar is marked to market inside end equity, so net pnl\n"
        "  differs from (end equity - start equity) by that unrealized amount.\n"
        "  Per-exit rows are the matched lots of those same closed trades.\n";

  reportExcursions(os);
  os << "=========================================================\n";
}

void Analytics::reportCurve(std::ostream &os, bool sharpeOk) {
  os << "\n-- Curve / Risk -----------------------------------------\n";

  const std::optional<double> totalReturn =
      startEquity != 0 ? std::optional<double>{(endEquity - startEquity) /
                                               startEquity}
                       : std::nullopt;

  std::optional<double> cagr;
  if (tradingDays > 0 && startEquity > 0 && endEquity > 0)
    cagr = std::pow(endEquity / startEquity, 252.0 / tradingDays) - 1;

  line(os, "trading days", std::to_string(tradingDays));
  line(os, "days in market",
       std::to_string(daysInMarket) + " (" +
           (tradingDays > 0
                ? pct(static_cast<double>(daysInMarket) / tradingDays)
                : "n/a") +
           ")");
  line(os, "start equity", money(startEquity));
  line(os, "end equity", money(endEquity));
  line(os, "total return", pctOrNa(totalReturn, true));
  line(os, "CAGR", pctOrNa(cagr, true));
  line(os, "max drawdown", pct(maxDrawDown));

  if (sharpeOk) {
    line(os, "mean daily return", pct(sharpeInfo.meanDailyReturns, true));
    line(os, "daily volatility", pct(std::sqrt(sharpeInfo.variance)));
    line(os, "annualized volatility",
         pct(std::sqrt(sharpeInfo.variance * 252)));
    line(os, "Sharpe", num(sharpeInfo.sharpe));
    line(os, "risk free (annual)", pct(sharpeInfo.riskFreeAnnual));
  } else {
    line(os, "sharpe / volatility",
         "n/a (need >= 2 bars with non-zero variance)");
  }
}

void Analytics::reportTradeStats(std::ostream &os, const std::string &label,
                                 const TradeStats &info) {
  os << "\n-- " << label << " ---------------------------------\n";

  const double net = info.grossProfit + info.grossLoss;
  const int losses = info.num - info.numWin - info.numBreakEven;
  // avgLoss is a magnitude; grossLoss itself is <= 0
  const std::optional<double> avgWin =
      info.numWin ? std::optional<double>{info.grossProfit / info.numWin}
                  : std::nullopt;
  const std::optional<double> avgLoss =
      losses ? std::optional<double>{-info.grossLoss / losses} : std::nullopt;

  std::optional<double> winLossRatio;
  if (avgWin && avgLoss && *avgLoss > 0)
    winLossRatio = *avgWin / *avgLoss;

  std::optional<double> profitFactor;
  if (info.grossLoss < 0)
    profitFactor = info.grossProfit / -info.grossLoss;

  // = winRate * avgWin - lossRate * avgLoss
  const std::optional<double> expectancy =
      info.num ? std::optional<double>{net / info.num} : std::nullopt;

  line(os, "trades", std::to_string(info.num));
  line(os, "win rate",
       info.num ? pct(static_cast<double>(info.numWin) / info.num) + "  (" +
                      std::to_string(info.numWin) + "W / " +
                      std::to_string(losses) + "L / " +
                      std::to_string(info.numBreakEven) + "BE)"
                : "n/a");
  line(os, "gross profit", money(info.grossProfit));
  line(os, "gross loss", money(-info.grossLoss));
  line(os, "net pnl", money(net));
  line(os, "avg win", avgWin ? money(*avgWin) : "n/a");
  line(os, "avg loss", avgLoss ? money(*avgLoss) : "n/a");
  line(os, "win/loss ratio", winLossRatio ? num(*winLossRatio) : "n/a");
  line(os, "profit factor", profitFactor ? num(*profitFactor) : "n/a");
  line(os, "expectancy", expectancy ? money(*expectancy) : "n/a");
}

void Analytics::reportExcursions(std::ostream &os) {
  struct Bucket {
    std::string label;
    std::optional<bool> isLong;
    std::optional<Outcome> outcome;
  };

  const Bucket buckets[] = {
      {"overall", std::nullopt, std::nullopt},
      {"long", true, std::nullopt},
      {"short", false, std::nullopt},
      {"win", std::nullopt, Outcome::Win},
      {"loss", std::nullopt, Outcome::Loss},
      {"breakeven", std::nullopt, Outcome::BreakEven},
      {"long / win", true, Outcome::Win},
      {"long / loss", true, Outcome::Loss},
      {"long / breakeven", true, Outcome::BreakEven},
      {"short / win", false, Outcome::Win},
      {"short / loss", false, Outcome::Loss},
      {"short / breakeven", false, Outcome::BreakEven},
  };

  os << "\n-- MAE / MFE by bucket (fraction of entry notional) ------\n";
  os << "  " << std::left << std::setw(17) << "bucket" << std::right
     << std::setw(7) << "n" << std::setw(11) << "MAE p50" << std::setw(11)
     << "MAE p90" << std::setw(11) << "MAE p95" << std::setw(11) << "MFE p50"
     << std::setw(11) << "MFE p75" << std::setw(11) << "MFE p90" << '\n';

  for (const auto &b : buckets) {
    const ExcursionSummary s =
        summarize(filter(excursions, b.isLong, b.outcome));

    // a bucket with no trades has nothing to say
    if (s.n == 0)
      continue;

    os << "  " << std::left << std::setw(17) << b.label << std::right
       << std::setw(7) << s.n << std::setw(11) << pctOrNa(s.maeMedian, true)
       << std::setw(11) << pctOrNa(s.maeP90, true) << std::setw(11)
       << pctOrNa(s.maeP95, true) << std::setw(11)
       << pctOrNa(s.mfeMedian, true) << std::setw(11)
       << pctOrNa(s.mfeP75, true) << std::setw(11)
       << pctOrNa(s.mfeP90, true) << '\n';
  }

  os << "\n  MAE is signed: negative = adverse excursion at its worst point.\n"
        "  Percentiles are upper tail, i.e. MAE p90 = a drawdown no worse than\n"
        "  this in 90% of trades. n/a = too few samples for that percentile\n"
        "  (needs 2/(1-p): 20 for p90, 40 for p95). Empty buckets are omitted.\n";
}
