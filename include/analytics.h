#pragma once

#include "state.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iostream>
#include <limits>
#include <optional>
#include <string>
#include <vector>

// outcome of a closed trade
enum class Outcome { Loss, BreakEven, Win };

inline Outcome classify(double pnl) {
  if (pnl > 0)
    return Outcome::Win;
  return pnl < 0 ? Outcome::Loss : Outcome::BreakEven;
}

// precondition: v is sorted ascending
inline std::optional<double> median(const std::vector<double> &v) {
  if (v.empty())
    return std::nullopt;
  const std::size_t n = v.size();
  return (n % 2) ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

// upper tail quantile: p fraction of the sample is <= the result
// precondition: v is sorted ascending
// enforce minNeeded so we have statistically valuable percentile result
inline std::optional<double> percentile(const std::vector<double> &v,
                                        double p) {
  if (v.empty() || p < 0 || p > 1)
    return std::nullopt;
  if (p >= 1.0)
    return v.back();

  // 1/(1-p) can land an ULP above its true integer (1-0.9 = 0.0999...8,
  // so 1/(1-0.9) = 10.000...2); without the epsilon ceil would demand
  // 22 samples for p90 instead of the documented 20
  const std::size_t minNeeded =
      std::ceil(1.0 / (1.0 - p) - 1e-9) * 2;
  const std::size_t n = v.size();
  if (n < minNeeded)
    return std::nullopt;

  const std::size_t k = std::clamp<std::size_t>(std::ceil(p * n), 1, n);
  return v[k - 1];
}

struct Excursion {
  double maeNorm;
  double mfeNorm;
  bool isLong;
  Outcome outcome;
};

struct OpenPosition {
  std::string date;
  int direction;
  int qty;
  double price;
};

struct EpisodeState {
  std::string openTime{};
  std::deque<OpenPosition> openPositions{};
  double entryNotional{0};
  double exitNotional{0};
  double avgEntryPrice{0};
  double mae{std::numeric_limits<double>::infinity()};
  double mfe{-std::numeric_limits<double>::infinity()};
};

struct ExitRecord {
  std::string entryTime;
  std::string exitTime;

  int direction;
  int qty;

  double exitPrice, entryPrice, pnl;
};

struct PositionRecord {
  std::string openTime, closeTime;
  int direction;
  double entryNotional, exitNotional, pnl, mae, mfe;
};

// trade-close metrics, shared by per-position and per-exit accounting
// grossProfit >= 0, grossLoss <= 0 (it is a sum of losing pnl)
// numLoss = num - numWin - numBreakEven
struct TradeStats {
  int numWin{0};
  int numBreakEven{0};
  int num{0};
  double grossProfit{0};
  double grossLoss{0};
};

struct SharpeInfo {
  std::vector<double> dailyReturns{};
  double meanDailyReturns{};
  double riskFreeAnnual{0};
  double variance{};
  double sharpe{};
};



class Analytics {
  std::vector<Excursion> excursions;
  std::vector<ExitRecord> exitRecords;
  std::vector<PositionRecord> positionRecords;

  TradeStats positionInfo{};
  TradeStats exitsInfo{};

  // max drawdown
  double maxDrawDown{0};
  double runningPeak{-std::numeric_limits<double>::infinity()};

  // Sharpe / Volatility
  SharpeInfo sharpeInfo{};

  // time in market
  int tradingDays{0};
  int daysInMarket{0};

  // CAGR
  double startEquity{};
  double endEquity{};

public:
  bool captureState(const State &state);

  void recordInfo(const std::vector<Execution> &executions,
                  const std::vector<BarSnapshot> &barSnapshots);

  void handleExecution(const Execution &execution, EpisodeState &episodeState);

  bool computeSharpe();

  void computePositionRecords();

  void computeExitRecords();
  
  
  // runs every compute step, then prints the full metric rundown
  void report(std::ostream &os = std::cout);

  void reportPositions() {
    std::cout << "size:" << positionRecords.size() << '\n';
    for (auto &p : positionRecords) {
      std::cout << "Open time:" << p.openTime << " Close time:" << p.closeTime
                << " Direction:" << p.direction << " Entry:" << p.entryNotional
                << " Exit:" << p.exitNotional << " PNL:" << p.pnl << '\n';
    }
  }

  void reportExits() {
    std::cout << "size:" << exitRecords.size() << '\n';
    for (auto &e : exitRecords) {
      std::cout << "Entry time:" << e.entryTime << " Exit time:" << e.exitTime
                << " Qty:" << e.qty << " Entry price:" << e.entryPrice
                << " Exit price:" << e.exitPrice << " pnl:" << e.pnl << '\n';
    }
  }



private:
  void reportCurve(std::ostream &os, bool sharpeOk);

  void reportTradeStats(std::ostream &os, const std::string &label,
                        const TradeStats &info);

  void reportExcursions(std::ostream &os);
};
