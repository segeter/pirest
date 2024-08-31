// clang-format off
#include "pch.h"
// clang-format on

#include <pirest/http_server.hpp>

using namespace pirest;

TEST(HttpServerTest, TestDestructor) {
  { HttpPlainServer server; }

  {
    HttpPlainServer server;
    server.ListenAndServe("0.0.0.0", 0);
  }
}

TEST(HttpServerTest, TestLogic) {
  HttpPlainServer server;
  for (auto i = 0; i < 3; ++i) {
    server.Close();
  }

  server.ListenAndServe("0.0.0.0", 0);
  for (auto i = 0; i < 3; ++i) {
    server.Close();
  }

  server.ListenAndServe("0.0.0.0", 0);
}