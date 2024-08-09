#include <cstdlib>
#include <pirest/http_cors_filter.hpp>
#include <pirest/http_server.hpp>
#include <unordered_map>

using namespace pirest;

using status = boost::beast::http::status;

class AuthorizationFilter : public HttpFilter {
 public:
  const char* name() const noexcept override { return "AuthFilter"; }

  Result OnIncomingRequest(const HttpConnection::Ptr& conn) override {
    auto& req = conn->request();
    if (req.target().starts_with("/user/login")) {
      return Result::kPassed;
    }
    if (std::rand() % 100 < 30) {
      conn->Respond(status::unauthorized, "Auth failed", "text/plain");
      return Result::kResponded;
    }
    return Result::kPassed;
  }
};

static std::uint64_t channel_id = 0;
static std::mutex mutex;
static std::unordered_map<std::uint64_t, std::string> channel_map;

static void UserLogin(const HttpConnection::Ptr& conn) {
  conn->Respond(status::ok, "Login success", "text/plain");
}

static void AddChannel(const HttpConnection::Ptr& conn) {
  std::lock_guard lock(mutex);
  auto id = ++channel_id;
  channel_map[id] = conn->ReleaseBody();
  conn->Respond(status::ok, std::to_string(id), "text/plain");
}

static void DeleteChannel(const HttpConnection::Ptr& conn, std::uint64_t id) {
  std::lock_guard lock(mutex);
  channel_map.erase(id);
  conn->Respond(status::ok);
}

static void UpdateChannel(const HttpConnection::Ptr& conn, std::uint64_t id) {
  std::lock_guard lock(mutex);
  auto it = channel_map.find(id);
  if (it != channel_map.end()) {
    it->second = conn->request().body();
  }
  conn->Respond(status::ok);
}

static void GetChannelList(const HttpConnection::Ptr& conn) {
  std::ostringstream oss;
  std::lock_guard lock(mutex);
  for (const auto& it : channel_map) {
    oss << it.first << ":" << it.second << ",";
  }
  auto body = oss.str();
  if (body.size() > 0) {
    body.pop_back();
  }
  conn->Respond(status::ok, std::move(body), "text/plain");
}

static void GetChannel(const HttpConnection::Ptr& conn, std::uint64_t id) {
  std::lock_guard lock(mutex);
  auto it = channel_map.find(id);
  if (it == channel_map.end()) {
    return conn->Respond(status::not_found,
                         "Not found channel " + std::to_string(id),
                         "text/plain");
  }
  return conn->Respond(status::ok, it->second, "text/plain");
}

static void Echo(const HttpConnection::Ptr& conn, std::string&& data,
                 std::optional<std::string>&& p) {
  auto& req = conn->request();
  std::ostringstream oss;
  oss << req.method_string() << " " << req.target() << std::endl;
  oss << "data=" << data << std::endl;
  if (p) {
    oss << "p=" << *p << std::endl;
  } else {
    oss << "p=<std::nullopt>" << std::endl;
  }
  oss << "body=" << req.body() << std::endl;
  conn->Respond(status::ok, oss.str(), "text/plain");
}

void TestHttpServer(std::stop_token st, const std::string& address,
                    std::uint16_t port) {
  auto server = std::make_shared<HttpDetectServer>();

  {
    auto filter = std::make_shared<HttpCorsFilter>();
    filter->set_allow_any_origins(true)
        .set_allow_methods({"POST", "GET", "PUT", "DELETE", "OPTIONS"})
        .set_allow_any_headers(true)
        .set_expose_headers({"authorization"});
    server->setting().AddFilter(filter).AddFilter(
        std::make_shared<AuthorizationFilter>());
  }

  server->HandleFunc("/user/login", &UserLogin, {"POST"});
  server->HandleFunc("/channel", &AddChannel, {"POST"});
  server->HandleFunc("/channel/{id}", &DeleteChannel, {"DELETE"});
  server->HandleFunc("/channel/{id}", &UpdateChannel, {"PUT"});
  server->HandleFunc("/channel", &GetChannelList, {"GET"});
  server->HandleFunc("/channel/{id}", &GetChannel, {"GET"});
  server->HandleFunc("/echo/{}?p", &Echo, {"GET", "POST"});

  std::srand((unsigned int)std::time(nullptr));

  server->ListenAndServe(address, port);

  while (!st.stop_requested()) {
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);
  }

  server->Close();
}