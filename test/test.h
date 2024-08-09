#pragma once
#include <stop_token>
#include <string>

void TestHttpRouter(std::stop_token st);

void TestHttpServer(std::stop_token st, const std::string& address,
                    std::uint16_t port);
