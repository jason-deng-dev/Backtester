#pragma once

#include <cstdint>
#include <string>

struct Bar {
  std::string date{};
  double close{}, high{}, low{}, open{};
  std::int64_t volume{};


  bool operator==(const Bar b2) const {
    return date == b2.date && close == b2.close &&
           high == b2.high && low == b2.low && open == b2.open &&
           volume == b2.volume;
  }
};

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
