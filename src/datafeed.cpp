#include "datafeed.h"
#include <algorithm>

bool DataFeed::load(const std::string &filePath) {
  std::ifstream in(filePath);
  if (!in)
    return false;

  std::error_code ec;
  auto fileSize = std::filesystem::file_size(filePath, ec);
  if (ec) {
    std::cerr << "file size access failed\n";
    return false;
  }

  buffer.resize(fileSize);

  // copy the next fileSize bytes from the file stream into buffer.data()
  in.read(buffer.data(), fileSize);
  if (!in)
    return false;

  barCount_ = std::count(buffer.begin(), buffer.end(), '\n');

  char *currStart = buffer.data();
  char *bufEnd = buffer.data() + buffer.size();

  // skip first 3 header bars
  for (int i = 0; i < 3; ++i) {
    auto found = std::find(currStart, bufEnd, '\n');
    if (found == bufEnd)
      return false;
    currStart = found + 1;
  }

  // set cursor to start of data
  cursor = currStart;

  return true;
}

bool DataFeed::next(Bar &bar) {
  const char *bufEnd = buffer.data() + buffer.size();

  if (cursor >= bufEnd) {
    return false;
  }

  // need to grab the values and change bar to reflect current bar
  // and then increment cursor to go to next bar (could be '\n' or end of
  // buffer)
  const char *lineStart = cursor;

  const char *lineEnd = std::find(lineStart, bufEnd, '\n');
  const char *dateEnd = std::find(lineStart, lineEnd, ',');

  bar.date.assign(lineStart, dateEnd);

  auto [ptr1, ec1] = std::from_chars(dateEnd + 1, lineEnd, bar.close);
  auto [ptr2, ec2] = std::from_chars(ptr1 + 1, lineEnd, bar.high);
  auto [ptr3, ec3] = std::from_chars(ptr2 + 1, lineEnd, bar.low);
  auto [ptr4, ec4] = std::from_chars(ptr3 + 1, lineEnd, bar.open);
  auto [ptr5, ec5] = std::from_chars(ptr4 + 1, lineEnd, bar.volume);

  if (ec1 != std::errc{} || ec2 != std::errc{} || ec3 != std::errc{} ||
      ec4 != std::errc{} || ec5 != std::errc{}) {
    std::cerr << "extraction of bar failed\n";
    return false;
  }

  if (ptr5 != lineEnd) {
    std::cerr << "end of extraction not at lineEnd\n";
    return false;
  }

  cursor = (lineEnd == bufEnd) ? bufEnd : lineEnd + 1;

  return true;
}
