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
  State(double cash_, int netQty_) : cash{cash_}, netQty{netQty_} {
    barSnapshots.push_back({"before trading", cash, netQty_, 0, 0});
  }

  double getEquity(double price) const {
    return cash + netQty*price;
  }
  double getCash() const { return cash; }
  int getNetQty() const { return netQty; }
  void setTotalBars(std::size_t bars) {totalBars = bars;}
  std::size_t getTotalBars() const {return totalBars;}

  // called on each bar
  void addBarSnapshot(const std::string &date, double price, double minPrice, double maxPrice) {
    barSnapshots.push_back({date, price * netQty + cash, netQty, minPrice, maxPrice});
  }

  void addExecution(const std::string &date, int qty, double price) {
    cash -= qty * price;
    netQty += qty;
    executions.push_back({date, qty, price});
  }



  const std::vector<BarSnapshot> &getBarSnapshots() const { return barSnapshots; }

  const std::vector<Execution> &getExecutions() const { return executions; }

private:
  std::size_t totalBars{};
  double cash{};
  int netQty{};
  std::vector<BarSnapshot> barSnapshots;
  std::vector<Execution> executions;
};
