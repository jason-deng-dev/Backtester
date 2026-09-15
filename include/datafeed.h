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
};

class DataFeed {
public:

  bool load(const std::string &filePath);
  bool next(Bar &bar);
  std::size_t barCount() const {return barCount_;}

private:
  std::vector<char> buffer{}; // owns the data + lifetime
  const char *cursor{};       // where next() resumes;
  std::size_t barCount_{};
};
