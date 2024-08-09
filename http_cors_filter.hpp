#pragma once
#include <boost/algorithm/string.hpp>
#include <boost/beast/core/string_type.hpp>
#include <pirest/http_filter.hpp>
#include <pirest/http_utils.hpp>
#include <vector>

namespace pirest {

class HttpCorsFilter : public HttpFilter {
 public:
  const char* name() const noexcept override { return "CorsFilter"; }

  Result OnIncomingRequest(const HttpConnection::Ptr& conn) override {
    conn->set_allow_origin("");
    auto& req = conn->request();
    if (req.method() == boost::beast::http::verb::options) {
      return HandleOptions(conn);
    }
    auto it = req.find(boost::beast::http::field::origin);
    if (it == req.end()) {
      return Result::kPassed;
    }
    auto allowed_origin = VerifyOrigin(it->value());
    if (allowed_origin.empty()) {
      conn->Respond(boost::beast::http::status::forbidden, "Origin not allowed",
                    "text/plain", false);
      return Result::kResponded;
    }
    conn->set_allow_origin(allowed_origin);
    return Result::kPassed;
  }

  void OnOutgingResponse(const HttpConnection::Ptr& conn,
                         boost::beast::http::response_header<>& resp) override {
    if (conn->allow_origin().size() > 0) {
      resp.set(boost::beast::http::field::access_control_allow_origin,
               conn->allow_origin());
      if (allow_any_headers_) {
        resp.set(boost::beast::http::field::access_control_allow_headers, "*");
      } else if (allow_headers_string_.size() > 0) {
        resp.set(boost::beast::http::field::access_control_allow_headers,
                 allow_headers_string_);
      }
      resp.set(boost::beast::http::field::access_control_allow_methods,
               allow_methods_string_);
      resp.set(boost::beast::http::field::access_control_max_age, max_age_);
      if (expose_headers_string_.size() > 0) {
        resp.set(boost::beast::http::field::access_control_expose_headers,
                 expose_headers_string_);
      }
    }
  }

  HttpCorsFilter& set_allow_origins(
      const std::vector<std::string>& allow_origins) noexcept {
    allow_origins_ = allow_origins;
    for (auto& origin : allow_origins_) {
      auto pos = origin.find(":80");
      if (pos == std::string::npos) {
        pos = origin.find(":443");
      }
      if (pos != std::string::npos) {
        origin = origin.substr(0, pos);
      }
      ToLower(origin);
    }
    return *this;
  }

  HttpCorsFilter& set_allow_headers(
      const std::vector<std::string>& allow_headers) noexcept {
    allow_headers_ = allow_headers;
    allow_headers_string_ = "";
    for (auto& header : allow_headers_) {
      ToLower(header);
      allow_headers_string_.append(header);
      allow_headers_string_.append(",");
    }
    if (allow_headers_string_.size() > 0) {
      allow_headers_string_.pop_back();
    }
    return *this;
  }

  HttpCorsFilter& set_allow_methods(
      const std::vector<std::string>& allow_methods) noexcept {
    allow_methods_ = allow_methods;
    allow_methods_string_ = "";
    for (auto& method : allow_methods_) {
      ToUpper(method);
      allow_methods_string_.append(method);
      allow_methods_string_.append(",");
    }
    if (allow_methods_string_.size() > 0) {
      allow_methods_string_.pop_back();
    }
    return *this;
  }

  HttpCorsFilter& set_expose_headers(
      const std::vector<std::string>& expose_headers) noexcept {
    expose_headers_ = expose_headers;
    expose_headers_string_ = "";
    for (const auto& header : expose_headers_) {
      expose_headers_string_.append(header);
      expose_headers_string_.append(",");
    }
    if (expose_headers_string_.size() > 0) {
      expose_headers_string_.pop_back();
    }
    return *this;
  }

  HttpCorsFilter& set_max_age(std::int32_t max_age) noexcept {
    max_age_ = std::to_string(max_age);
    return *this;
  }

  HttpCorsFilter& set_allow_credentials(bool allow_credentials) noexcept {
    allow_credentials_ = allow_credentials;
    return *this;
  }

  HttpCorsFilter& set_allow_any_origins(bool allow_any_origins) noexcept {
    allow_any_origins_ = allow_any_origins;
    return *this;
  }

  HttpCorsFilter& set_allow_any_headers(bool allow_any_headers) noexcept {
    allow_any_headers_ = allow_any_headers;
    return *this;
  }

 private:
  Result HandleOptions(const HttpConnection::Ptr& conn) const {
    auto& req = conn->request();
    boost::beast::http::response<boost::beast::http::empty_body> resp;
    resp.version(req.version());
    auto origin = req[boost::beast::http::field::origin];
    if (origin.size() > 0) {
      auto request_method =
          req[boost::beast::http::field::access_control_request_method];
      if (request_method.empty()) {
        resp.result(boost::beast::http::status::bad_request);
      } else {
        auto request_headers =
            req[boost::beast::http::field::access_control_request_headers];
        auto allowed_origin =
            Preflight(origin, request_method, request_headers);
        if (allowed_origin.size() > 0) {
          conn->set_allow_origin(allowed_origin);
          resp.result(boost::beast::http::status::ok);
        } else {
          resp.result(boost::beast::http::status::forbidden);
        }
      }
    } else {
      resp.result(boost::beast::http::status::ok);
      resp.set(boost::beast::http::field::allow, "*");
      resp.set(boost::beast::http::field::age, "3600");
    }
    conn->Respond(std::move(resp));
    return Result::kResponded;
  }

  std::string VerifyOrigin(boost::beast::string_view origin) const {
    std::string allowed_origin;
    if (allow_any_origins_) {
      allowed_origin = "*";
    } else {
      auto pos = origin.find(":80");
      if (pos == std::string::npos) {
        pos = origin.find(":443");
      }
      if (pos != std::string::npos) {
        allowed_origin = origin.substr(0, pos);
      } else {
        allowed_origin = origin;
      }
      ToLower(allowed_origin);
      auto it = std::find_if(allow_origins_.begin(), allow_origins_.end(),
                             [&allowed_origin](const std::string& item) {
                               return item == allowed_origin;
                             });
      if (it == allow_origins_.end()) {
        allowed_origin = "";
      } else {
        allowed_origin = origin;
      }
    }
    return allowed_origin;
  }

  std::string Preflight(boost::beast::string_view origin,
                        boost::beast::string_view request_method,
                        boost::beast::string_view request_headers) const {
    auto allowed_origin = VerifyOrigin(origin);
    if (allowed_origin.empty()) {
      return "";
    }
    {
      std::string req_method = request_method;
      ToUpper(req_method);
      auto it = std::find_if(allow_methods_.begin(), allow_methods_.end(),
                             [&req_method](const std::string& method) {
                               return method == req_method;
                             });
      if (it == allow_methods_.end()) {
        return "";
      }
    }
    if (!allow_any_headers_ && request_headers.size() > 0) {
      std::string req_headers = request_headers;
      TrimAllSpace(req_headers);
      ToLower(req_headers);
      std::vector<std::string> vec;
      boost::split(vec, req_headers, boost::is_any_of(","));
      for (const auto& header : vec) {
        auto it = std::find_if(
            allow_headers_.begin(), allow_headers_.end(),
            [&header](const std::string& item) { return item == header; });
        if (it == allow_headers_.end()) {
          return "";
        }
      }
    }
    return allowed_origin;
  }

 private:
  std::vector<std::string> allow_origins_;
  std::vector<std::string> allow_headers_;
  std::string allow_headers_string_;
  std::vector<std::string> allow_methods_;
  std::string allow_methods_string_;
  std::vector<std::string> expose_headers_;
  std::string expose_headers_string_;
  std::string max_age_ = "3600";
  bool allow_credentials_ = false;
  bool allow_any_origins_ = false;
  bool allow_any_headers_ = false;
};

}  // namespace pirest
