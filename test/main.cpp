#include <csignal>
#include <iostream>

#include "test.h"

std::stop_source stop;

static void CtrlHandler(std::int32_t sig) {
  signal(SIGINT, CtrlHandler);
  stop.request_stop();
  stop = {};
}

int main() {
  signal(SIGINT, CtrlHandler);

  TestHttpRouter(stop.get_token());

  TestHttpServer(stop.get_token(), "0.0.0.0", 8087);
}
