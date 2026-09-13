#ifndef STATE_H
#define STATE_H

#include <string>
#include <vector>

struct Equity {
  std::string date{};
  double equity{};
  int netQty{};
};

struct Execution {
  std::string date{};
  int qty{};
  double price{};
};


class State {
public:
  State(double cash_, int netQty_) : cash{cash_}, netQty{netQty_} {}

  double getCash() const { return cash; }
  int getNetQty() const { return netQty; }

  // called on each bar
  void addEquity(const std::string &date, double price) {
    equityCurve.push_back({date, price * netQty + cash, netQty});
  }

  void addExecution(const std::string &date, int qty, double price) {
    cash -= qty * price;
    netQty += qty;
    executions.push_back({date, qty, price});
  }



  const std::vector<Equity> &getEquityCurve() const { return equityCurve; }

  const std::vector<Execution> &getExecutions() const { return executions; }

private:
  double cash{};
  int netQty{};
  std::vector<Equity> equityCurve;
  std::vector<Execution> executions;
};

#endif
