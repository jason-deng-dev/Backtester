#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct BarSnapshot {
  std::string date{};
  double equity{};
  int netQty{};
  double minPrice{};
  double maxPrice{};
};

struct Execution {
  std::string date{};
  int qty{};
  double price{};
};

class State {
public:
  State(double cash_ = 0, int netQty_ = 0) : cash{cash_}, netQty{netQty_} {
    barSnapshots.push_back({"before trading", cash, netQty_, 0, 0});
  }

  double getEquity(double price) const { return cash + netQty * price; }
  double getCash() const { return cash; }
  int getNetQty() const { return netQty; }
  void setTotalBars(std::size_t bars) { totalBars = bars; }
  std::size_t getTotalBars() const { return totalBars; }

  // called on each bar
  void addBarSnapshot(const std::string &date, double price, double minPrice,
                      double maxPrice) {
    barSnapshots.push_back(
        {date, price * netQty + cash, netQty, minPrice, maxPrice});
  }

  void addExecution(const std::string &date, int qty, double price) {

    int netQtyBefore = netQty;

    cash -= qty * price;
    netQty += qty;
    executions.push_back({date, qty, price});

    // Cases:
    // Long/Short increase/decrease position
    // Long/Short reverse

    bool longIncreasing = netQtyBefore < netQty && netQtyBefore >= 0;
    bool shortIncreasing = netQtyBefore > netQty && netQtyBefore <= 0;
    bool reverse = (netQtyBefore < 0 && netQty > 0 )|| (netQtyBefore > 0 && netQty < 0);

    
    if (longIncreasing || shortIncreasing) {
      avgEntryPrice =
          (avgEntryPrice * entryQty + price * qty) / (entryQty + qty);
      entryQty += qty;
    }
    else if (reverse) {
      avgEntryPrice = netQty*price/netQty;
      entryQty = netQty;
    }
    else if (netQty == 0) {
      entryQty = 0;
      avgEntryPrice = 0;
    }
    else { // reduction case
      entryQty += qty;
    }
  }
  double getAvgEntryPrice() const { return avgEntryPrice; }

  const std::vector<BarSnapshot> &getBarSnapshots() const {
    return barSnapshots;
  }

  const std::vector<Execution> &getExecutions() const { return executions; }

private:
  /*
    avgEntr = Σ(fill_price_i × fill_qty_i) / Σ(fill_qty_i)
    newAvg = (oldAvg * oldQty + fillPrice * fillQty) / oldQty+fillQty

  */
  int entryQty = 0;
  double avgEntryPrice{0};

  std::size_t totalBars{};
  double cash{};
  int netQty{};
  std::vector<BarSnapshot> barSnapshots;
  std::vector<Execution> executions;
};
