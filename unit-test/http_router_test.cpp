// clang-format off
#include "pch.h"
// clang-format on

#include <pirest/http_connection.hpp>
#include <pirest/http_router.hpp>

using namespace pirest;
using namespace ::testing;

class HttpRouterTest : public ::testing::Test {
 protected:
  void SetUp() override {}
  void TearDown() override {}
  HttpRouter router;
  HttpConnection::Ptr conn;
};

TEST_F(HttpRouterTest, TestAddRouteOk) {
  router.AddRoute("/hello", [&](const HttpConnection::Ptr&) {}, {"GET"});
  router.AddRoute("/hello/{}", [&](const HttpConnection::Ptr&, int) {},
                  {"GET"});
  router.AddRoute("/hello/{}/{}", [&](const HttpConnection::Ptr&, int, int) {},
                  {"GET"});
  router.AddRoute("/{}/hello/{}/xxx",
                  [&](const HttpConnection::Ptr&, int, int) {}, {"GET"});
  router.AddRoute("/hello/{}/xxx?p1&p2",
                  [&](const HttpConnection::Ptr&, int, int, int) {}, {"GET"});
}

TEST_F(HttpRouterTest, TestAddRouteFail) {
  try {
    router.AddRoute("/hello", [&](const HttpConnection::Ptr&, int) {}, {"GET"});
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Number of parameters does not match");
  }

  try {
    router.AddRoute("/hello/{}", [&](const HttpConnection::Ptr&) {}, {"GET"});
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Number of parameters does not match");
  }

  try {
    router.AddRoute("/hello/{}?p1&p2",
                    [&](const HttpConnection::Ptr&, int, int, int, int) {},
                    {"GET"});
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Number of parameters does not match");
  }
}

TEST_F(HttpRouterTest, TestNoParameterRouting) {
  bool cb_flag = false;
  router.AddRoute("/hello", [&](const HttpConnection::Ptr&) { cb_flag = true; },
                  {"GET", "POST"});

  cb_flag = false;
  router.Routing(conn, "GET", "/hello");
  ASSERT_TRUE(cb_flag);

  cb_flag = false;
  router.Routing(conn, "POST", "/hello");
  ASSERT_TRUE(cb_flag);

  cb_flag = false;
  try {
    router.Routing(conn, "PUT", "/hello");
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Method not allowed");
  }
  ASSERT_FALSE(cb_flag);

  cb_flag = false;
  try {
    router.Routing(conn, "POST", "/hello1");
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Route not found");
  }
  ASSERT_FALSE(cb_flag);

  cb_flag = false;
  try {
    router.Routing(conn, "POST", "/hello/xxx");
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    ASSERT_STREQ(e.what(), "Route not found");
  }
  ASSERT_FALSE(cb_flag);
}

TEST_F(HttpRouterTest, TestArgumentMatchRouting) {
  int index = 0;
  router.AddRoute("/hello/{name}",
                  [&](const HttpConnection::Ptr&, std::string name) {
                    index = 1;
                    ASSERT_EQ(name, "kitty");
                  },
                  {"GET"});
  router.AddRoute(
      "/hello/{name}/world/{id}?require_str&opt_int&require_int",
      [&](const HttpConnection::Ptr&, std::string name, int id,
          std::string req_str, std::optional<int> opt_int, int req_int) {
        index = 2;
        ASSERT_EQ(name, "kitty");
        ASSERT_EQ(id, 888);
        ASSERT_EQ(req_str, "hello kitty");
        if (opt_int) {
          ASSERT_EQ(opt_int, 200);
        }
        ASSERT_EQ(req_int, 300);
      },
      {"GET"});
  router.AddRoute("/hello/{name}/world/{id}",
                  [&](const HttpConnection::Ptr&, std::string name, int id) {
                    index = 3;
                    ASSERT_EQ(name, "kitty");
                    ASSERT_EQ(id, 999);
                  },
                  {"GET"});

  index = 0;
  router.Routing(conn, "GET", "/hello/kitty");
  ASSERT_EQ(index, 1);

  index = 0;
  router.Routing(conn, "GET", "/hello/kitty/world/999");
  ASSERT_EQ(index, 3);

  index = 0;
  router.Routing(conn, "GET",
                 "/hello/kitty/world/"
                 "888?require_str=hello%20kitty&require_int=300&opt_int=200");
  ASSERT_EQ(index, 2);

  index = 0;
  router.Routing(conn, "GET",
                 "/hello/kitty/world/"
                 "888?require_str=hello%20kitty&require_int=300&opt_int=200&"
                 "ignore_other=&a123=aaa");
  ASSERT_EQ(index, 2);

  index = 0;
  router.Routing(
      conn, "GET",
      "/hello/kitty/world/888?require_str=hello%20kitty&require_int=300");
  ASSERT_EQ(index, 2);

  index = 0;
  try {
    router.Routing(conn, "GET", "/hello/kitty/world/not_number");
    FAIL() << "unreachable";
  } catch (const std::exception& e) {
    std::string_view ev{e.what()};
    auto pos = ev.find("bad lexical cast");
    ASSERT_NE(pos, ev.npos);
  }
  ASSERT_EQ(index, 0);
}
