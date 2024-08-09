#pragma once
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/context.hpp>
#include <pirest/http_connection_impl.hpp>
#include <pirest/http_router.hpp>

namespace pirest {

template <class CONNECTION>
class HttpBasicServer {
 public:
  using Ptr = std::shared_ptr<HttpBasicServer>;

  HttpBasicServer() noexcept
      : acceptor_(accept_io_.ctx), socket_(socket_io_.ctx) {}

  HttpSetting& setting() noexcept { return setting_; }

  template <class Function>
  void HandleFunc(const std::string& target, Function&& func,
                  const std::vector<std::string>& allowed_methods = {}) {
    router_.AddRoute(target, std::forward<Function>(func), allowed_methods);
  }

  void ListenAndServe(const std::string& address, std::uint16_t port) {
    boost::asio::ip::tcp::endpoint endpoint(
        boost::asio::ip::make_address(address), port);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();
    accept_io_.thread = std::thread([this]() { accept_io_.ctx.run(); });
    socket_io_.thread = std::thread([this]() { socket_io_.ctx.run(); });
    StartAccept();
  }

  void Close() noexcept {
    closed_ = true;
    accept_io_.ctx.stop();
    accept_io_.guard.reset();
    try {
      accept_io_.thread.join();
    } catch (const std::exception&) {
    }
    socket_io_.ctx.stop();
    socket_io_.guard.reset();
    try {
      socket_io_.thread.join();
    } catch (const std::exception&) {
    }
    acceptor_ = boost::asio::ip::tcp::acceptor(acceptor_.get_executor());
  }

 private:
  void StartAccept() {
    acceptor_.async_accept(
        socket_, [this](const boost::system::error_code& ec) {
          if (!ec) {
            boost::beast::tcp_stream stream(std::move(socket_));
            stream.expires_after(setting_.read_timeout);
            std::make_shared<CONNECTION>(std::move(stream),
                                         boost::beast::flat_buffer{}, ssl_ctx_,
                                         router_, setting_)
                ->Run();
          }
          if (!closed_) {
            StartAccept();
          }
        });
  }

 private:
  bool closed_ = false;
  struct {
    std::thread thread;
    boost::asio::io_context ctx{1};
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type>
        guard = boost::asio::make_work_guard(ctx);
  } accept_io_, socket_io_;
  boost::asio::ip::tcp::acceptor acceptor_;
  boost::asio::ip::tcp::socket socket_;
  boost::asio::ssl::context ssl_ctx_{boost::asio::ssl::context::tlsv12};
  HttpRouter router_;
  HttpSetting setting_;
};

// Only for http
using HttpPlainServer = HttpBasicServer<HttpPlainConnection>;

// Only for https
using HttpSslServer = HttpBasicServer<HttpSslConnection>;

// Both http and https(Automatic detection of http or https)
using HttpDetectServer = HttpBasicServer<HttpDetectConnection>;

}  // namespace pirest
