#pragma once
#include <pirest/http_connection.hpp>

namespace pirest {

class HttpFilter {
 public:
  enum class Result { kPassed, kResponded };

  virtual ~HttpFilter() noexcept {}

  virtual const char* name() const noexcept = 0;

  virtual Result OnIncomingRequest(const HttpConnection::Ptr& conn) = 0;

  virtual void OnOutgingResponse(const HttpConnection::Ptr& conn,
                                 boost::beast::http::response_header<>& resp) {}
};

}  // namespace pirest
