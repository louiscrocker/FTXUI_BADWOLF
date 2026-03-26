// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_DISTRIBUTED_HPP
#define FTXUI_UI_DISTRIBUTED_HPP

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/ui/json.hpp"
#include "ftxui/ui/live_source.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// ── RedisConfig
// ───────────────────────────────────────────────────────────────
struct RedisConfig {
  std::string host = "localhost";
  int port = 6379;
  std::string password;
  int db = 0;
  std::chrono::milliseconds connect_timeout{3000};
  std::chrono::milliseconds reconnect_delay{1000};
  int max_retries = -1;  // -1 = infinite
};

// ── Internal value-parsing helper
// ─────────────────────────────────────────────
namespace detail {
template <typename T>
T ParseRedisValue(const std::string& s);
template <>
inline std::string ParseRedisValue<std::string>(const std::string& s) {
  return s;
}
template <>
inline JsonValue ParseRedisValue<JsonValue>(const std::string& s) {
  return Json::ParseSafe(s);
}
}  // namespace detail

// ── RedisSource<T>
// ────────────────────────────────────────────────────────────
template <typename T = std::string>
class RedisSource : public LiveSource<T> {
 public:
  enum class Mode { Subscribe, PollKey, ListenQueue };

  static std::shared_ptr<RedisSource<T>> Subscribe(const std::string& channel,
                                                   RedisConfig cfg = {}) {
    auto src = std::shared_ptr<RedisSource<T>>(new RedisSource<T>());
    src->cfg_ = std::move(cfg);
    src->channel_or_key_ = channel;
    src->mode_ = Mode::Subscribe;
    return src;
  }

  static std::shared_ptr<RedisSource<T>> PollKey(
      const std::string& key,
      std::chrono::milliseconds interval = std::chrono::seconds(1),
      RedisConfig cfg = {}) {
    auto src = std::shared_ptr<RedisSource<T>>(new RedisSource<T>());
    src->cfg_ = std::move(cfg);
    src->channel_or_key_ = key;
    src->mode_ = Mode::PollKey;
    src->poll_interval_ = interval;
    return src;
  }

  static std::shared_ptr<RedisSource<T>> ListenQueue(
      const std::string& list_key,
      RedisConfig cfg = {}) {
    auto src = std::shared_ptr<RedisSource<T>>(new RedisSource<T>());
    src->cfg_ = std::move(cfg);
    src->channel_or_key_ = list_key;
    src->mode_ = Mode::ListenQueue;
    return src;
  }

  ~RedisSource() override { Stop(); }

  void Start() override {
    if (running_.exchange(true)) {
      return;
    }
    worker_ = std::thread([this] { WorkerLoop(); });
  }

  void Stop() override {
    running_.store(false);
    cv_.notify_all();
    if (worker_.joinable()) {
      worker_.join();
    }
    if (sock_ >= 0) {
      ::close(sock_);
      sock_ = -1;
    }
  }

  bool IsRunning() const override { return running_.load(); }

  std::string Name() const override {
    return "redis://" + cfg_.host + ":" + std::to_string(cfg_.port) + "/" +
           channel_or_key_;
  }

  std::string last_error() const {
    std::lock_guard<std::mutex> lock(err_mutex_);
    return last_error_;
  }

  bool IsConnected() const { return connected_.load(); }

  // ── RESP protocol helpers (public for tests) ────────────────────────────
  static std::string BuildCommand(const std::vector<std::string>& parts) {
    std::string cmd = "*" + std::to_string(parts.size()) + "\r\n";
    for (const auto& p : parts) {
      cmd += "$" + std::to_string(p.size()) + "\r\n" + p + "\r\n";
    }
    return cmd;
  }

  static std::vector<std::string> ParseResp(const std::string& resp) {
    std::vector<std::string> result;
    size_t pos = 0;
    ParseRespAt(resp, pos, result);
    return result;
  }

 protected:
  T Fetch() override { return T{}; }  // not used (push model)

 private:
  RedisConfig cfg_;
  std::string channel_or_key_;
  Mode mode_{Mode::Subscribe};
  std::chrono::milliseconds poll_interval_{1000};
  int sock_{-1};
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  mutable std::mutex err_mutex_;
  std::string last_error_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;

  RedisSource() = default;

  void SetError(const std::string& e) {
    std::lock_guard<std::mutex> lock(err_mutex_);
    last_error_ = e;
  }

  bool ConnectSocket() {
    if (sock_ >= 0) {
      ::close(sock_);
      sock_ = -1;
    }
    connected_.store(false);

    sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) {
      SetError("socket() failed");
      return false;
    }

    // Set timeouts
    struct timeval tv;
    tv.tv_sec = cfg_.connect_timeout.count() / 1000;
    tv.tv_usec = (cfg_.connect_timeout.count() % 1000) * 1000;
    ::setsockopt(sock_, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(sock_, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    int rc = ::getaddrinfo(cfg_.host.c_str(), std::to_string(cfg_.port).c_str(),
                           &hints, &res);
    if (rc != 0 || !res) {
      SetError(std::string("getaddrinfo failed: ") + gai_strerror(rc));
      ::close(sock_);
      sock_ = -1;
      return false;
    }
    int ret = ::connect(sock_, res->ai_addr, res->ai_addrlen);
    ::freeaddrinfo(res);
    if (ret < 0) {
      SetError("connect() failed");
      ::close(sock_);
      sock_ = -1;
      return false;
    }

    // AUTH
    if (!cfg_.password.empty()) {
      SendCommandRaw({"AUTH", cfg_.password});
      auto line = ReadLine();
      if (line.empty() || line[0] == '-') {
        SetError("AUTH failed: " + line);
        ::close(sock_);
        sock_ = -1;
        return false;
      }
    }
    // SELECT db
    if (cfg_.db != 0) {
      SendCommandRaw({"SELECT", std::to_string(cfg_.db)});
      auto line = ReadLine();
      if (line.empty() || line[0] == '-') {
        SetError("SELECT failed: " + line);
        ::close(sock_);
        sock_ = -1;
        return false;
      }
    }
    connected_.store(true);
    return true;
  }

  void SendCommandRaw(const std::vector<std::string>& cmd) {
    std::string data = BuildCommand(cmd);
    size_t total = 0;
    while (total < data.size()) {
      ssize_t n = ::send(sock_, data.c_str() + total, data.size() - total, 0);
      if (n <= 0) {
        return;
      }
      total += static_cast<size_t>(n);
    }
  }

  std::string ReadLine() {
    std::string line;
    char ch;
    while (running_.load()) {
      int n = static_cast<int>(::recv(sock_, &ch, 1, 0));
      if (n <= 0) {
        return "";
      }
      if (ch == '\n') {
        if (!line.empty() && line.back() == '\r') {
          line.pop_back();
        }
        return line;
      }
      line += ch;
    }
    return "";
  }

  std::string ReadBulkString(int len) {
    if (len < 0) {
      return "";
    }
    std::string buf(static_cast<size_t>(len), '\0');
    int total = 0;
    while (total < len && running_.load()) {
      int n = static_cast<int>(::recv(sock_, &buf[static_cast<size_t>(total)],
                                      static_cast<size_t>(len - total), 0));
      if (n <= 0) {
        return "";
      }
      total += n;
    }
    ReadLine();  // consume trailing \r\n
    return buf;
  }

  // Read one complete RESP value, return as string
  std::string ReadRespValue() {
    std::string line = ReadLine();
    if (line.empty()) {
      return "";
    }
    char type = line[0];
    std::string rest = line.substr(1);
    switch (type) {
      case '+':
        return rest;
      case '-':
        return rest;
      case ':':
        return rest;
      case '$': {
        int len = std::stoi(rest);
        if (len < 0) {
          return "";
        }
        return ReadBulkString(len);
      }
      case '*': {
        int count = std::stoi(rest);
        std::string last;
        for (int i = 0; i < count; i++) {
          last = ReadRespValue();
        }
        return last;
      }
      default:
        return line;
    }
  }

  void WorkerLoop() {
    int retries = 0;
    while (running_.load()) {
      if (!ConnectSocket()) {
        if (cfg_.max_retries >= 0 && retries >= cfg_.max_retries) {
          break;
        }
        retries++;
        Sleep(cfg_.reconnect_delay);
        continue;
      }
      retries = 0;
      bool ok = false;
      switch (mode_) {
        case Mode::Subscribe:
          ok = RunSubscribeLoop();
          break;
        case Mode::PollKey:
          ok = RunPollKeyLoop();
          break;
        case Mode::ListenQueue:
          ok = RunListenQueueLoop();
          break;
      }
      connected_.store(false);
      if (sock_ >= 0) {
        ::close(sock_);
        sock_ = -1;
      }
      (void)ok;
      if (running_.load()) {
        Sleep(cfg_.reconnect_delay);
      }
    }
  }

  bool RunSubscribeLoop() {
    SendCommandRaw({"SUBSCRIBE", channel_or_key_});
    // Read subscription confirmation (3-element array)
    ReadLine();       // *3
    ReadRespValue();  // "subscribe"
    ReadRespValue();  // channel name
    ReadRespValue();  // subscriber count
    // Now read messages
    while (running_.load()) {
      std::string line = ReadLine();
      if (line.empty()) {
        return false;
      }
      if (line != "*3") {
        continue;
      }
      std::string type = ReadRespValue();
      ReadRespValue();  // channel
      std::string payload = ReadRespValue();
      if (type == "message" || type == "pmessage") {
        this->EmitData(detail::ParseRedisValue<T>(payload));
      }
    }
    return true;
  }

  bool RunPollKeyLoop() {
    std::string last;
    while (running_.load()) {
      SendCommandRaw({"GET", channel_or_key_});
      std::string line = ReadLine();
      if (line.empty()) {
        return false;
      }
      std::string value;
      if (line[0] == '$') {
        int len = std::stoi(line.substr(1));
        if (len >= 0) {
          value = ReadBulkString(len);
        }
      }
      if (value != last) {
        last = value;
        if (!value.empty()) {
          this->EmitData(detail::ParseRedisValue<T>(value));
        }
      }
      Sleep(poll_interval_);
    }
    return true;
  }

  bool RunListenQueueLoop() {
    while (running_.load()) {
      SendCommandRaw({"BLPOP", channel_or_key_, "0"});
      // Response: *2\r\n$keylen\r\nkey\r\n$valuelen\r\nvalue\r\n
      std::string line = ReadLine();
      if (line.empty()) {
        return false;
      }
      if (!line.empty() && line[0] == '*') {
        ReadRespValue();  // key
        std::string value = ReadRespValue();
        if (!value.empty()) {
          this->EmitData(detail::ParseRedisValue<T>(value));
        }
      }
    }
    return true;
  }

  void Sleep(std::chrono::milliseconds dur) {
    std::unique_lock<std::mutex> lock(cv_mutex_);
    cv_.wait_for(lock, dur, [this] { return !running_.load(); });
  }

  // ── Static RESP parser for ParseResp() ──────────────────────────────────
  static void ParseRespAt(const std::string& resp,
                          size_t& pos,
                          std::vector<std::string>& out) {
    if (pos >= resp.size()) {
      return;
    }
    size_t end = resp.find("\r\n", pos);
    if (end == std::string::npos) {
      return;
    }
    std::string line = resp.substr(pos, end - pos);
    pos = end + 2;
    if (line.empty()) {
      return;
    }
    char type = line[0];
    std::string rest = line.substr(1);
    switch (type) {
      case '+':
      case '-':
      case ':':
        out.push_back(rest);
        break;
      case '$': {
        int len = std::stoi(rest);
        if (len < 0) {
          out.push_back("");
        } else {
          out.push_back(resp.substr(pos, static_cast<size_t>(len)));
          pos += static_cast<size_t>(len) + 2;  // skip \r\n
        }
        break;
      }
      case '*': {
        int count = std::stoi(rest);
        for (int i = 0; i < count; i++) {
          ParseRespAt(resp, pos, out);
        }
        break;
      }
      default:
        break;
    }
  }
};

// ── KafkaConfig
// ───────────────────────────────────────────────────────────────
struct KafkaConfig {
  std::string brokers = "localhost:9092";
  std::string group_id = "badwolf";
  std::string client_id = "ftxui-badwolf";
  int64_t offset = -1;  // -1 = latest, -2 = earliest
  std::chrono::milliseconds poll_timeout{100};
  std::chrono::milliseconds reconnect_delay{2000};
};

// ── KafkaSource
// ───────────────────────────────────────────────────────────────
class KafkaSource : public LiveSource<std::string> {
 public:
  KafkaSource(std::string topic, KafkaConfig cfg = {});
  ~KafkaSource() override;

  void Start() override;
  void Stop() override;
  bool IsRunning() const override;
  std::string Name() const override;
  std::string last_error() const;
  bool IsConnected() const;

  static constexpr int16_t API_KEY_FETCH = 1;
  static constexpr int16_t API_VERSION = 0;
  static constexpr int32_t CORRELATION_ID = 42;

  // Exposed for tests
  std::vector<uint8_t> BuildFetchRequest(int64_t offset);

 protected:
  std::string Fetch() override { return {}; }

 private:
  std::string topic_;
  KafkaConfig cfg_;
  int sock_{-1};
  std::thread worker_;
  std::atomic<bool> running_{false};
  std::atomic<bool> connected_{false};
  mutable std::mutex err_mutex_;
  std::string last_error_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  int64_t current_offset_{0};

  void SetError(const std::string& e);
  bool ConnectToBroker(const std::string& broker);
  std::vector<std::string> ParseFetchResponse(const std::vector<uint8_t>& data);
  void PollLoop();
  void Sleep(std::chrono::milliseconds dur);
};

// ── GrpcStreamConfig
// ──────────────────────────────────────────────────────────
struct GrpcStreamConfig {
  std::string host = "localhost";
  int port = 50051;
  std::string service;
  std::string method;
  std::string request_json = "{}";
  std::chrono::milliseconds reconnect_delay{2000};
};

// ── GrpcStreamSource
// ──────────────────────────────────────────────────────────
class GrpcStreamSource : public LiveSource<JsonValue> {
 public:
  explicit GrpcStreamSource(GrpcStreamConfig cfg = {});
  ~GrpcStreamSource() override;

  void Start() override;
  void Stop() override;
  bool IsRunning() const override;
  std::string Name() const override;
  std::string last_error() const;

 protected:
  JsonValue Fetch() override { return {}; }

 private:
  GrpcStreamConfig cfg_;
  int sock_{-1};
  std::thread worker_;
  std::atomic<bool> running_{false};
  mutable std::mutex err_mutex_;
  std::string last_error_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;

  void SetError(const std::string& e);
  void StreamLoop();
  std::vector<uint8_t> ReadGrpcFrame();
  void WriteGrpcFrame(const std::vector<uint8_t>& payload);
  void Sleep(std::chrono::milliseconds dur);
};

// ── DistributedState<T>
// ───────────────────────────────────────────────────────
template <typename T>
class DistributedState {
 public:
  DistributedState(std::string key,
                   std::shared_ptr<LiveSource<T>> source,
                   T initial = T{})
      : key_(std::move(key)),
        source_(std::move(source)),
        local_(std::move(initial)) {
    if (source_) {
      source_->OnData([this](const T& val) {
        local_.Set(val);
        synced_ = true;
      });
      if (!source_->IsRunning()) {
        source_->Start();
      }
    }
  }

  const T& Get() const { return local_.Get(); }

  void Set(const T& val) { local_.Set(val); }

  int OnChange(std::function<void(const T&)> fn) {
    return local_.OnChange(std::move(fn));
  }

  void RemoveOnChange(int id) { local_.RemoveOnChange(id); }

  std::string key() const { return key_; }

  bool IsSynced() const { return synced_; }

  std::string SourceName() const { return source_ ? source_->Name() : ""; }

 private:
  std::string key_;
  std::shared_ptr<LiveSource<T>> source_;
  Reactive<T> local_;
  bool synced_{false};
};

// ── DistributedDashboard
// ──────────────────────────────────────────────────────
class DistributedDashboard {
 public:
  DistributedDashboard& AddSource(const std::string& title,
                                  std::shared_ptr<LiveSource<std::string>> src);
  DistributedDashboard& AddJsonSource(
      const std::string& title,
      std::shared_ptr<LiveSource<JsonValue>> src);
  DistributedDashboard& WithLayout(int cols = 2);

  ftxui::Component Build() const;

 private:
  struct Entry {
    std::string title;
    std::shared_ptr<LiveSource<std::string>> string_src;
    std::shared_ptr<LiveSource<JsonValue>> json_src;
    bool is_json{false};
  };
  std::vector<Entry> entries_;
  int cols_{2};
};

// ── Free functions
// ────────────────────────────────────────────────────────────
inline std::shared_ptr<RedisSource<std::string>> RedisSubscribe(
    const std::string& channel,
    RedisConfig cfg = {}) {
  return RedisSource<std::string>::Subscribe(channel, std::move(cfg));
}

inline std::shared_ptr<RedisSource<JsonValue>> RedisSubscribeJson(
    const std::string& channel,
    RedisConfig cfg = {}) {
  return RedisSource<JsonValue>::Subscribe(channel, std::move(cfg));
}

inline std::shared_ptr<RedisSource<std::string>> RedisPollKey(
    const std::string& key,
    std::chrono::milliseconds interval = std::chrono::seconds(1),
    RedisConfig cfg = {}) {
  return RedisSource<std::string>::PollKey(key, interval, std::move(cfg));
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_DISTRIBUTED_HPP
