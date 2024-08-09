#pragma once
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/detect_ssl.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <pirest/http_connection.hpp>
#include <pirest/http_filter.hpp>
#include <pirest/http_router.hpp>
#include <pirest/http_setting.hpp>

namespace pirest {

using HttpParser = boost::beast::http::request_parser<HttpBodyType>;

template <class D>
class HttpConnectionBase : public HttpConnection {
  using DerivedPtr = std::shared_ptr<D>;

 public:
  HttpConnectionBase(boost::beast::flat_buffer buffer, HttpRouter& router,
                     HttpSetting& setting) noexcept
      : buffer_(std::move(buffer)), router_(router), setting_(setting) {
    conn_variant_ = this;
  }

  void ReadRequest() { ReadRequest(Derived().shared_from_this()); }

  void ReadRequest(DerivedPtr&& self) {
    parser_.emplace();
    parser_->header_limit(setting_.header_limit);
    if (setting_.body_limit) {
      parser_->body_limit(*setting_.body_limit);
    } else {
      parser_->body_limit(boost::none);
    }
    boost::beast::http::async_read(
        Derived().stream(), buffer_, *parser_,
        [self = std::move(self)](const boost::beast::error_code& ec,
                                 std::size_t bytes_transferred) {
          self->OnRequest(self, ec, bytes_transferred);
        });
  }

  void OnRequest(const HttpConnection::Ptr& conn,
                 const boost::beast::error_code& ec,
                 std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if (ec) {
      if (ec == boost::beast::http::error::end_of_stream) {
        Derived().DoEof();
      }
    } else {
      Derived().ExpiresNever();
      request_ = parser_->release();
      for (const auto& filter : setting_.filters) {
        if (filter->OnIncomingRequest(conn) == HttpFilter::Result::kResponded) {
          return;
        }
      }
      try {
        router_.Routing(conn, request_.method_string(), request_.target());
      } catch (const std::exception& e) {
        return conn->Respond(boost::beast::http::status::bad_request, e.what(),
                             "text/plain", false);
      }
    }
  }

  template <class Body>
  void Respond(boost::beast::http::response<Body>&& resp) {
    if (!resp.has_content_length() && !resp.chunked()) {
      resp.content_length(0);
    }
    auto self = Derived().shared_from_this();
    for (const auto& filter : setting_.filters) {
      filter->OnOutgingResponse(self, resp);
    }
    auto ptr =
        std::make_unique<boost::beast::http::response<Body>>(std::move(resp));
    auto p = ptr.get();
    boost::beast::http::async_write(
        Derived().stream(), *p,
        [self = std::move(self), ptr = std::move(ptr),
         close = !p->keep_alive()](const boost::beast::error_code& ec,
                                   std::size_t bytes_transferred) mutable {
          ptr.reset();
          self->OnWrite(self, close, ec, bytes_transferred);
        });
  }

  void OnWrite(DerivedPtr& self, bool close, const boost::beast::error_code& ec,
               std::size_t bytes_transferred) {
    boost::ignore_unused(bytes_transferred);
    if (!ec) {
      if (close) {
        Derived().DoEof();
      } else {
        Derived().ExpiresAfter(setting_.read_timeout);
        ReadRequest(std::move(self));
      }
    }
  }

 private:
  D& Derived() noexcept { return reinterpret_cast<D&>(*this); }

 protected:
  std::optional<HttpParser> parser_;
  boost::beast::flat_buffer buffer_;
  HttpRouter& router_;
  HttpSetting& setting_;
};

class HttpPlainConnection
    : public HttpConnectionBase<HttpPlainConnection>,
      public std::enable_shared_from_this<HttpPlainConnection> {
 public:
  HttpPlainConnection(boost::beast::tcp_stream stream,
                      boost::beast::flat_buffer buffer,
                      boost::asio::ssl::context&, HttpRouter& router,
                      HttpSetting& setting) noexcept
      : HttpConnectionBase(std::move(buffer), router, setting),
        stream_(std::move(stream)) {}

  void Run() { ReadRequest(); }

  boost::beast::tcp_stream& stream() noexcept { return stream_; }

  void ExpiresAfter(const std::chrono::milliseconds& time) {
    stream_.expires_after(time);
  }

  void ExpiresNever() { stream_.expires_never(); }

  void DoEof() {
    boost::beast::error_code ec;
    stream_.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
  }

 private:
  boost::beast::tcp_stream stream_;
};

class HttpSslConnection
    : public HttpConnectionBase<HttpSslConnection>,
      public std::enable_shared_from_this<HttpSslConnection> {
 public:
  HttpSslConnection(boost::beast::tcp_stream stream,
                    boost::beast::flat_buffer buffer,
                    boost::asio::ssl::context& ssl_ctx, HttpRouter& router,
                    HttpSetting& setting) noexcept
      : HttpConnectionBase(std::move(buffer), router, setting),
        stream_(std::move(stream), ssl_ctx) {}

  void Run() {
    stream_.async_handshake(
        boost::asio::ssl::stream_base::server, buffer_.data(),
        [this, self = shared_from_this()](const boost::beast::error_code& ec,
                                          std::size_t bytes_used) mutable {
          if (!ec) {
            buffer_.consume(bytes_used);
            ReadRequest(std::move(self));
          }
        });
  }

  boost::beast::ssl_stream<boost::beast::tcp_stream>& stream() noexcept {
    return stream_;
  }

  void ExpiresAfter(const std::chrono::milliseconds& time) {
    stream_.next_layer().expires_after(time);
  }

  void ExpiresNever() { stream_.next_layer().expires_never(); }

  void DoEof() {
    stream_.async_shutdown([](const boost::beast::error_code& ec) {});
  }

 private:
  boost::beast::ssl_stream<boost::beast::tcp_stream> stream_;
};

class HttpDetectConnection
    : public std::enable_shared_from_this<HttpDetectConnection> {
 public:
  HttpDetectConnection(boost::beast::tcp_stream stream,
                       boost::beast::flat_buffer buffer,
                       boost::asio::ssl::context& ssl_ctx, HttpRouter& router,
                       HttpSetting& setting) noexcept
      : stream_(std::move(stream)),
        buffer_(std::move(buffer)),
        ssl_ctx_(ssl_ctx),
        router_(router),
        setting_(setting) {}

  void Run() {
    boost::beast::async_detect_ssl(
        stream_, buffer_,
        [this, self = shared_from_this()](const boost::beast::error_code& ec,
                                          bool is_ssl) {
          if (ec) {
            return;
          }
          if (is_ssl) {
            std::make_shared<HttpSslConnection>(std::move(stream_),
                                                std::move(buffer_), ssl_ctx_,
                                                router_, setting_)
                ->Run();
          } else {
            std::make_shared<HttpPlainConnection>(std::move(stream_),
                                                  std::move(buffer_), ssl_ctx_,
                                                  router_, setting_)
                ->Run();
          }
        });
  }

 private:
  boost::beast::tcp_stream stream_;
  boost::beast::flat_buffer buffer_;
  boost::asio::ssl::context& ssl_ctx_;
  HttpRouter& router_;
  HttpSetting& setting_;
};

}  // namespace pirest
