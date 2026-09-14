#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "state.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct Execursion {
  double maeNorm;
  double mfeNorm;
  bool isLong;
  bool win;
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

struct PositionsInfo {
  // trade-close metrics
  int numWin{0};
  int num{0};
  double grossProfit{0};
  double grossLoss{0};
};

struct ExitsInfo {
  int numWin{0};
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
  std::vector<Execursion> excursions;
  std::vector<ExitRecord> exitRecords;
  std::vector<PositionRecord> positionRecords;

  PositionsInfo positionInfo{};
  ExitsInfo exitsInfo{};

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
};

#endif
