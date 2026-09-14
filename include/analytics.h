#ifndef ANALYTICS_H
#define ANALYTICS_H

#include "state.h"
#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

struct OpenPosition {
  std::string date;
  int direction;
  int qty;
  double price;
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
  double entryNotional, exitNotional, pnl;
};

struct TradeCloseInfo {
  // trade-close metrics
  int numWinningPositions{0};
  int numPositions{0};

  int numWinningExits{0};
  int numExits{0};

  double positionGrossProfit{0};
  double positionGrossLoss{0};

  double exitGrossProfit{0};
  double exitGrossLoss{0};
};

struct SharpeInfo {
  std::vector<double> dailyReturns{};
  double meanDailyReturns{};
  double riskFreeAnnual{};
  double variance{};
  double sharpe{};
};

class Analytics {
  std::vector<ExitRecord> exitRecords;
  std::vector<PositionRecord> positionRecords;

  TradeCloseInfo tradeCloseMetrics{};

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
  void captureState(const State &state);

  void recordInfo(const std::vector<Execution> &executions, const std::vector<BarSnapshot> &barSnapshots);

  

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
      std::cout << "Entry time:" << e.entryTime << " Exit time:" << e.exitTime << " Qty:" << e.qty
                << " Entry price:" << e.entryPrice
                << " Exit price:" << e.exitPrice << " pnl:" << e.pnl << '\n';
    }
  }
};

#endif
