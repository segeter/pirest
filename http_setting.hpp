#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pirest {

class HttpFilter;

struct HttpSetting {
  using FilterList = std::vector<std::shared_ptr<HttpFilter>>;
  std::uint32_t header_limit = 8 * 1024;
  std::optional<std::uint64_t> body_limit = 1024 * 1024;
  std::chrono::milliseconds read_timeout = std::chrono::seconds(60);
  FilterList filters;

  HttpSetting& set_header_limit(std::uint32_t val) noexcept {
    header_limit = val;
    return *this;
  }

  HttpSetting& set_body_limit(
      const std::optional<std::uint64_t>& val) noexcept {
    body_limit = val;
    return *this;
  }

  HttpSetting& set_read_timeout(const std::chrono::milliseconds& val) noexcept {
    read_timeout = val;
    return *this;
  }

  HttpSetting& AddFilter(const std::shared_ptr<HttpFilter>& filter) {
    filters.emplace_back(filter);
    return *this;
  }
};

}  // namespace pirest
