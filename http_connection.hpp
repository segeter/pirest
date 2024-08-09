#pragma once
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/status.hpp>
#include <boost/beast/http/string_body.hpp>
#include <variant>

namespace pirest {

using HttpBodyType = boost::beast::http::string_body;

using HttpRequest = boost::beast::http::request<HttpBodyType>;

using HttpHeaderList = std::vector<std::pair<std::string, std::string>>;

class HttpPlainConnection;
class HttpSslConnection;
template <class>
class HttpConnectionBase;
class HttpConnection {
 public:
  using Ptr = std::shared_ptr<HttpConnection>;

  const HttpRequest& request() const noexcept { return request_; }

  std::string ReleaseBody() noexcept { return std::move(request_.body()); }

  template <class Body>
  void Respond(boost::beast::http::response<Body>&& resp) {
    std::visit([&resp](const auto& conn) { conn->Respond(std::move(resp)); },
               conn_variant_);
  }

  void Respond(boost::beast::http::status status,
               const HttpHeaderList& headers = {}) {
    Respond(status, request_.keep_alive(), headers);
  }

  void Respond(boost::beast::http::status status, bool keep_alive,
               const HttpHeaderList& headers = {}) {
    boost::beast::http::response<boost::beast::http::empty_body> resp(
        status, request_.version());
    resp.content_length(0);
    resp.keep_alive(keep_alive);
    for (const auto& pair : headers) {
      resp.set(pair.first, pair.second);
    }
    Respond(std::move(resp));
  }

  void Respond(boost::beast::http::status status, std::string&& body,
               const char* content_type, const HttpHeaderList& headers = {}) {
    Respond(status, std::move(body), content_type, request_.keep_alive(),
            headers);
  }

  void Respond(boost::beast::http::status status, std::string&& body,
               const char* content_type, bool keep_alive,
               const HttpHeaderList& headers = {}) {
    boost::beast::http::response<boost::beast::http::string_body> resp(
        status, request_.version());
    resp.keep_alive(keep_alive);
    resp.set(boost::beast::http::field::content_type, content_type);
    for (const auto& pair : headers) {
      resp.set(pair.first, pair.second);
    }
    resp.body() = std::move(body);
    resp.prepare_payload();
    Respond(std::move(resp));
  }

  void Respond(boost::beast::http::status status, const std::string& body,
               const char* content_type, const HttpHeaderList& headers = {}) {
    Respond(status, body, content_type, request_.keep_alive(), headers);
  }

  void Respond(boost::beast::http::status status, const std::string& body,
               const char* content_type, bool keep_alive,
               const HttpHeaderList& headers = {}) {
    boost::beast::http::response<boost::beast::http::string_body> resp(
        status, request_.version());
    resp.keep_alive(keep_alive);
    resp.set(boost::beast::http::field::content_type, content_type);
    for (const auto& pair : headers) {
      resp.set(pair.first, pair.second);
    }
    resp.body() = body;
    resp.prepare_payload();
    Respond(std::move(resp));
  }

  void Respond(boost::beast::http::status status, const char* body,
               const char* content_type, const HttpHeaderList& headers = {}) {
    Respond(status, body, content_type, request_.keep_alive(), headers);
  }

  void Respond(boost::beast::http::status status, const char* body,
               const char* content_type, bool keep_alive,
               const HttpHeaderList& headers = {}) {
    boost::beast::http::response<boost::beast::http::string_body> resp(
        status, request_.version());
    resp.keep_alive(keep_alive);
    resp.set(boost::beast::http::field::content_type, content_type);
    for (const auto& pair : headers) {
      resp.set(pair.first, pair.second);
    }
    resp.body() = body;
    resp.prepare_payload();
    Respond(std::move(resp));
  }

  void Respond(boost::beast::http::status status, const char* body,
               std::size_t size, const char* content_type,
               const HttpHeaderList& headers = {}) {
    Respond(status, body, size, content_type, request_.keep_alive(), headers);
  }

  void Respond(boost::beast::http::status status, const char* body,
               std::size_t size, const char* content_type, bool keep_alive,
               const HttpHeaderList& headers = {}) {
    boost::beast::http::response<boost::beast::http::string_body> resp(
        status, request_.version());
    resp.keep_alive(keep_alive);
    resp.set(boost::beast::http::field::content_type, content_type);
    for (const auto& pair : headers) {
      resp.set(pair.first, pair.second);
    }
    resp.body().append(body, size);
    resp.prepare_payload();
    Respond(std::move(resp));
  }

  void set_allow_origin(const std::string& origin) noexcept {
    allow_origin_ = origin;
  }

  const std::string& allow_origin() const noexcept { return allow_origin_; }

 protected:
  HttpRequest request_;
  std::string allow_origin_;
  std::variant<HttpConnectionBase<HttpPlainConnection>*,
               HttpConnectionBase<HttpSslConnection>*>
      conn_variant_;
};

}  // namespace pirest

#include <pirest/http_connection_impl.hpp>
