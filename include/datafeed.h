#pragma once

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

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

class DataFeed {
public:
  bool load(const std::string &filePath);
  bool next(Bar &bar);
  std::size_t barCount() const { return barCount_; }

private:
  std::vector<char> buffer{}; // owns the data + lifetime
  const char *cursor{};       // where next() resumes;
  std::size_t barCount_{};
};
