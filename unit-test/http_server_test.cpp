// clang-format off
#include "pch.h"
// clang-format on

#include <pirest/http_server.hpp>

using namespace pirest;

TEST(HttpServerTest, TestDestructor) {
  { auto _ = std::make_shared<HttpPlainServer>(); }

  {
    auto server = std::make_shared<HttpPlainServer>();
    server->ListenAndServe("0.0.0.0", 0);
  }
}

TEST(HttpServerTest, TestLogic) {
  auto server = std::make_shared<HttpPlainServer>();
  for (auto i = 0; i < 3; ++i) {
    server->Close();
  }

  server->ListenAndServe("0.0.0.0", 0);
  for (auto i = 0; i < 3; ++i) {
    server->Close();
  }

  server->ListenAndServe("0.0.0.0", 0);
}