// clang-format off
#include "pch.h"
// clang-format on

#include <pirest/http_server.hpp>

using namespace pirest;

class HttpServerTest : public ::testing::Test {};

TEST_F(HttpServerTest, TestMultipleClose) {
  auto server = std::make_shared<HttpPlainServer>();
  server->Close();
  server->Close();
  server->Close();
}