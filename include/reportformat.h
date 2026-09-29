#pragma once

// Text formatting shared by the report writers (Analytics::report and
// MonteCarlo::reportAggregateStats) so both print the same numbers the same
// way. Keep this free of any analytics or monte carlo types.

#include <iomanip>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>

namespace reportfmt {

// width of the label column, so values line up across every report
inline constexpr int kLabelWidth = 22;

inline void line(std::ostream &os, const std::string &label,
                 const std::string &value) {
  os << "  " << std::left << std::setw(kLabelWidth) << label << value << '\n';
}

// 1234567.5 -> "$1,234,567.50"
inline std::string money(double v) {
  std::ostringstream raw;
  raw << std::fixed << std::setprecision(2) << std::abs(v);
  std::string s = raw.str();
  for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(s.find('.')) - 3; i > 0;
       i -= 3) {
    s.insert(i, ",");
  }
  return (v < 0 ? "-$" : "$") + s;
}

// v is a fraction (0.05 -> "5.00%")
inline std::string pct(double v, bool sign = false) {
  std::ostringstream os;
  if (sign)
    os << std::showpos;
  os << std::fixed << std::setprecision(2) << v * 100 << "%";
  return os.str();
}

inline std::string num(double v) {
  std::ostringstream os;
  os << std::fixed << std::setprecision(2) << v;
  return os.str();
}

inline std::string pctOrNa(const std::optional<double> &v, bool sign = false) {
  return v ? pct(*v, sign) : "n/a";
}

} // namespace reportfmt
