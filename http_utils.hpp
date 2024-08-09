#pragma once
#include <algorithm>
#include <string>

namespace pirest {

static void ToLower(std::string& str) noexcept {
  std::transform(str.begin(), str.end(), str.begin(), tolower);
}

static void ToUpper(std::string& str) noexcept {
  std::transform(str.begin(), str.end(), str.begin(), toupper);
}

static void TrimAllSpace(std::string& str) noexcept {
  str.erase(remove_if(str.begin(), str.end(), isspace), str.end());
}

}  // namespace pirest
