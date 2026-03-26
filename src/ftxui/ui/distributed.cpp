// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/distributed.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/json.hpp"

namespace ftxui::ui {

// ── KafkaSource
// ────────────────────────────────────────────────────────────────

KafkaSource::KafkaSource(std::string topic, KafkaConfig cfg)
    : topic_(std::move(topic)), cfg_(std::move(cfg)) {
  current_offset_ = 0;
}

KafkaSource::~KafkaSource() {
  Stop();
}

void KafkaSource::Start() {
  if (running_.exchange(true)) {
    return;
  }
  current_offset_ = 0;
  worker_ = std::thread([this] { PollLoop(); });
}

void KafkaSource::Stop() {
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

bool KafkaSource::IsRunning() const {
  return running_.load();
}

std::string KafkaSource::Name() const {
  return "kafka://" + cfg_.brokers + "/" + topic_;
}

std::string KafkaSource::last_error() const {
  std::lock_guard<std::mutex> lock(err_mutex_);
  return last_error_;
}

bool KafkaSource::IsConnected() const {
  return connected_.load();
}

void KafkaSource::SetError(const std::string& e) {
  std::lock_guard<std::mutex> lock(err_mutex_);
  last_error_ = e;
}

// Big-endian integer helpers
static void WriteInt32BE(std::vector<uint8_t>& buf, int32_t val) {
  buf.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void WriteInt16BE(std::vector<uint8_t>& buf, int16_t val) {
  buf.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  buf.push_back(static_cast<uint8_t>(val & 0xFF));
}

static void WriteInt64BE(std::vector<uint8_t>& buf, int64_t val) {
  for (int i = 7; i >= 0; i--) {
    buf.push_back(static_cast<uint8_t>((val >> (8 * i)) & 0xFF));
  }
}

static void WriteString16BE(std::vector<uint8_t>& buf, const std::string& s) {
  WriteInt16BE(buf, static_cast<int16_t>(s.size()));
  buf.insert(buf.end(), s.begin(), s.end());
}

static int32_t ReadInt32BE(const std::vector<uint8_t>& buf, size_t& pos) {
  if (pos + 4 > buf.size()) {
    return 0;
  }
  int32_t val = (static_cast<int32_t>(buf[pos]) << 24) |
                (static_cast<int32_t>(buf[pos + 1]) << 16) |
                (static_cast<int32_t>(buf[pos + 2]) << 8) |
                static_cast<int32_t>(buf[pos + 3]);
  pos += 4;
  return val;
}

static int16_t ReadInt16BE(const std::vector<uint8_t>& buf, size_t& pos) {
  if (pos + 2 > buf.size()) {
    return 0;
  }
  int16_t val = static_cast<int16_t>((static_cast<int16_t>(buf[pos]) << 8) |
                                     static_cast<int16_t>(buf[pos + 1]));
  pos += 2;
  return val;
}

static int64_t ReadInt64BE(const std::vector<uint8_t>& buf, size_t& pos) {
  if (pos + 8 > buf.size()) {
    return 0;
  }
  int64_t val = 0;
  for (int i = 0; i < 8; i++) {
    val = (val << 8) | static_cast<int64_t>(buf[pos + static_cast<size_t>(i)]);
  }
  pos += 8;
  return val;
}

std::vector<uint8_t> KafkaSource::BuildFetchRequest(int64_t offset) {
  std::vector<uint8_t> payload;
  WriteInt16BE(payload, API_KEY_FETCH);
  WriteInt16BE(payload, API_VERSION);
  WriteInt32BE(payload, CORRELATION_ID);
  WriteString16BE(payload, cfg_.client_id);
  WriteInt32BE(payload, -1);   // replica_id = -1
  WriteInt32BE(payload, 500);  // max_wait_ms
  WriteInt32BE(payload, 1);    // min_bytes
  WriteInt32BE(payload, 1);    // num_topics
  WriteString16BE(payload, topic_);
  WriteInt32BE(payload, 1);  // num_partitions
  WriteInt32BE(payload, 0);  // partition
  WriteInt64BE(payload, offset);
  WriteInt32BE(payload, 1048576);  // max_bytes

  std::vector<uint8_t> request;
  WriteInt32BE(request, static_cast<int32_t>(payload.size()));
  request.insert(request.end(), payload.begin(), payload.end());
  return request;
}

std::vector<std::string> KafkaSource::ParseFetchResponse(
    const std::vector<uint8_t>& data) {
  std::vector<std::string> messages;
  if (data.size() < 8) {
    return messages;
  }
  size_t pos = 0;
  /* int32 correlation_id = */ ReadInt32BE(data, pos);
  /* int32 num_topics = */ ReadInt32BE(data, pos);
  /* skip topic name */ {
    int16_t tlen = ReadInt16BE(data, pos);
    pos += static_cast<size_t>(tlen > 0 ? tlen : 0);
  }
  /* int32 num_partitions = */ ReadInt32BE(data, pos);
  /* int32 partition = */ ReadInt32BE(data, pos);
  /* int16 error_code = */ ReadInt16BE(data, pos);
  /* int64 high_watermark = */ ReadInt64BE(data, pos);
  int32_t msg_set_size = ReadInt32BE(data, pos);
  if (msg_set_size <= 0 || pos >= data.size()) {
    return messages;
  }

  size_t msg_set_end = pos + static_cast<size_t>(msg_set_size);
  while (pos + 12 < msg_set_end && pos < data.size()) {
    int64_t msg_offset = ReadInt64BE(data, pos);
    int32_t msg_size = ReadInt32BE(data, pos);
    if (msg_size <= 0 || pos + static_cast<size_t>(msg_size) > data.size()) {
      break;
    }
    size_t msg_end = pos + static_cast<size_t>(msg_size);
    /* crc = */ ReadInt32BE(data, pos);
    /* magic = */ pos++;
    /* attributes = */ pos++;
    // key
    int32_t key_len = ReadInt32BE(data, pos);
    if (key_len > 0) {
      pos += static_cast<size_t>(key_len);
    }
    // value
    int32_t val_len = ReadInt32BE(data, pos);
    if (val_len > 0 && pos + static_cast<size_t>(val_len) <= msg_end) {
      messages.push_back(std::string(reinterpret_cast<const char*>(&data[pos]),
                                     static_cast<size_t>(val_len)));
      current_offset_ = msg_offset + 1;
    }
    pos = msg_end;
  }
  return messages;
}

bool KafkaSource::ConnectToBroker(const std::string& broker) {
  if (sock_ >= 0) {
    ::close(sock_);
    sock_ = -1;
  }
  connected_.store(false);

  std::string host = broker;
  int port = 9092;
  auto colon = broker.rfind(':');
  if (colon != std::string::npos) {
    host = broker.substr(0, colon);
    try {
      port = std::stoi(broker.substr(colon + 1));
    } catch (...) {
    }
  }

  sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock_ < 0) {
    SetError("socket() failed");
    return false;
  }

  struct addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo* res = nullptr;
  int rc =
      ::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res);
  if (rc != 0 || !res) {
    SetError(std::string("getaddrinfo: ") + gai_strerror(rc));
    ::close(sock_);
    sock_ = -1;
    return false;
  }
  int ret = ::connect(sock_, res->ai_addr, res->ai_addrlen);
  ::freeaddrinfo(res);
  if (ret < 0) {
    SetError("connect() failed to " + broker);
    ::close(sock_);
    sock_ = -1;
    return false;
  }
  connected_.store(true);
  return true;
}

void KafkaSource::Sleep(std::chrono::milliseconds dur) {
  std::unique_lock<std::mutex> lock(cv_mutex_);
  cv_.wait_for(lock, dur, [this] { return !running_.load(); });
}

void KafkaSource::PollLoop() {
  // Extract first broker from comma-separated list
  std::string broker = cfg_.brokers;
  auto comma = cfg_.brokers.find(',');
  if (comma != std::string::npos) {
    broker = cfg_.brokers.substr(0, comma);
  }

  while (running_.load()) {
    if (!ConnectToBroker(broker)) {
      Sleep(cfg_.reconnect_delay);
      continue;
    }

    bool need_reconnect = false;
    while (running_.load() && !need_reconnect) {
      auto req = BuildFetchRequest(current_offset_);

      // Send request
      {
        size_t total = 0;
        while (total < req.size()) {
          ssize_t n = ::send(sock_, req.data() + total, req.size() - total, 0);
          if (n <= 0) {
            need_reconnect = true;
            break;
          }
          total += static_cast<size_t>(n);
        }
      }
      if (need_reconnect) {
        break;
      }

      // Read response length
      {
        uint8_t len_buf[4];
        int got = 0;
        while (got < 4) {
          int n = static_cast<int>(
              ::recv(sock_, len_buf + got, static_cast<size_t>(4 - got), 0));
          if (n <= 0) {
            need_reconnect = true;
            break;
          }
          got += n;
        }
        if (need_reconnect) {
          break;
        }

        int32_t resp_len = (static_cast<int32_t>(len_buf[0]) << 24) |
                           (static_cast<int32_t>(len_buf[1]) << 16) |
                           (static_cast<int32_t>(len_buf[2]) << 8) |
                           static_cast<int32_t>(len_buf[3]);
        if (resp_len <= 0 || resp_len > 10 * 1024 * 1024) {
          need_reconnect = true;
          break;
        }

        // Read response body
        std::vector<uint8_t> resp_data(static_cast<size_t>(resp_len));
        size_t recv_total = 0;
        while (recv_total < static_cast<size_t>(resp_len)) {
          int n = static_cast<int>(::recv(sock_, resp_data.data() + recv_total,
                                          resp_data.size() - recv_total, 0));
          if (n <= 0) {
            need_reconnect = true;
            break;
          }
          recv_total += static_cast<size_t>(n);
        }
        if (need_reconnect) {
          break;
        }

        auto msgs = ParseFetchResponse(resp_data);
        for (auto& msg : msgs) {
          EmitData(msg);
        }
      }
      Sleep(cfg_.poll_timeout);
    }

    connected_.store(false);
    ::close(sock_);
    sock_ = -1;
    if (running_.load()) {
      Sleep(cfg_.reconnect_delay);
    }
  }
}

// ── GrpcStreamSource
// ──────────────────────────────────────────────────────────

GrpcStreamSource::GrpcStreamSource(GrpcStreamConfig cfg)
    : cfg_(std::move(cfg)) {}

GrpcStreamSource::~GrpcStreamSource() {
  Stop();
}

void GrpcStreamSource::Start() {
  if (running_.exchange(true)) {
    return;
  }
  worker_ = std::thread([this] { StreamLoop(); });
}

void GrpcStreamSource::Stop() {
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

bool GrpcStreamSource::IsRunning() const {
  return running_.load();
}

std::string GrpcStreamSource::Name() const {
  return "grpc://" + cfg_.host + ":" + std::to_string(cfg_.port) + "/" +
         cfg_.service + "/" + cfg_.method;
}

std::string GrpcStreamSource::last_error() const {
  std::lock_guard<std::mutex> lock(err_mutex_);
  return last_error_;
}

void GrpcStreamSource::SetError(const std::string& e) {
  std::lock_guard<std::mutex> lock(err_mutex_);
  last_error_ = e;
}

void GrpcStreamSource::Sleep(std::chrono::milliseconds dur) {
  std::unique_lock<std::mutex> lock(cv_mutex_);
  cv_.wait_for(lock, dur, [this] { return !running_.load(); });
}

void GrpcStreamSource::WriteGrpcFrame(const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> frame(5 + payload.size());
  frame[0] = 0;  // not compressed
  uint32_t len = static_cast<uint32_t>(payload.size());
  frame[1] = (len >> 24) & 0xFF;
  frame[2] = (len >> 16) & 0xFF;
  frame[3] = (len >> 8) & 0xFF;
  frame[4] = len & 0xFF;
  std::copy(payload.begin(), payload.end(), frame.begin() + 5);
  size_t total = 0;
  while (total < frame.size()) {
    ssize_t n = ::send(sock_, frame.data() + total, frame.size() - total, 0);
    if (n <= 0) {
      return;
    }
    total += static_cast<size_t>(n);
  }
}

std::vector<uint8_t> GrpcStreamSource::ReadGrpcFrame() {
  uint8_t header[5];
  size_t got = 0;
  while (got < 5 && running_.load()) {
    int n = static_cast<int>(::recv(sock_, header + got, 5 - got, 0));
    if (n <= 0) {
      return {};
    }
    got += static_cast<size_t>(n);
  }
  uint32_t length = (static_cast<uint32_t>(header[1]) << 24) |
                    (static_cast<uint32_t>(header[2]) << 16) |
                    (static_cast<uint32_t>(header[3]) << 8) |
                    static_cast<uint32_t>(header[4]);
  if (length == 0 || length > 64 * 1024 * 1024) {
    return {};
  }
  std::vector<uint8_t> buf(length);
  size_t total = 0;
  while (total < length && running_.load()) {
    int n = static_cast<int>(
        ::recv(sock_, buf.data() + total, buf.size() - total, 0));
    if (n <= 0) {
      return {};
    }
    total += static_cast<size_t>(n);
  }
  return buf;
}

void GrpcStreamSource::StreamLoop() {
  while (running_.load()) {
    if (sock_ >= 0) {
      ::close(sock_);
      sock_ = -1;
    }
    sock_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sock_ < 0) {
      SetError("socket() failed");
      Sleep(cfg_.reconnect_delay);
      continue;
    }
    struct addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    struct addrinfo* res = nullptr;
    int rc = ::getaddrinfo(cfg_.host.c_str(), std::to_string(cfg_.port).c_str(),
                           &hints, &res);
    if (rc != 0 || !res) {
      SetError(std::string("getaddrinfo: ") + gai_strerror(rc));
      ::close(sock_);
      sock_ = -1;
      Sleep(cfg_.reconnect_delay);
      continue;
    }
    int ret = ::connect(sock_, res->ai_addr, res->ai_addrlen);
    ::freeaddrinfo(res);
    if (ret < 0) {
      SetError("connect() failed");
      ::close(sock_);
      sock_ = -1;
      Sleep(cfg_.reconnect_delay);
      continue;
    }

    // Build HTTP/1.1 POST request for gRPC-Web
    std::string path = "/" + cfg_.service + "/" + cfg_.method;
    std::vector<uint8_t> body_payload(cfg_.request_json.begin(),
                                      cfg_.request_json.end());
    std::vector<uint8_t> grpc_body(5 + body_payload.size());
    grpc_body[0] = 0;
    uint32_t blen = static_cast<uint32_t>(body_payload.size());
    grpc_body[1] = (blen >> 24) & 0xFF;
    grpc_body[2] = (blen >> 16) & 0xFF;
    grpc_body[3] = (blen >> 8) & 0xFF;
    grpc_body[4] = blen & 0xFF;
    std::copy(body_payload.begin(), body_payload.end(), grpc_body.begin() + 5);

    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << cfg_.host << ":" << cfg_.port << "\r\n"
        << "Content-Type: application/grpc-web+json\r\n"
        << "Accept: application/grpc-web+json\r\n"
        << "Content-Length: " << grpc_body.size() << "\r\n"
        << "Connection: keep-alive\r\n"
        << "\r\n";
    std::string req_str = req.str();
    ::send(sock_, req_str.c_str(), req_str.size(), 0);
    ::send(sock_, grpc_body.data(), grpc_body.size(), 0);

    // Read HTTP response headers
    std::string headers;
    char ch;
    bool header_done = false;
    while (!header_done && running_.load()) {
      int n = static_cast<int>(::recv(sock_, &ch, 1, 0));
      if (n <= 0) {
        break;
      }
      headers += ch;
      if (headers.size() >= 4 &&
          headers.substr(headers.size() - 4) == "\r\n\r\n") {
        header_done = true;
      }
    }
    if (!header_done) {
      ::close(sock_);
      sock_ = -1;
      Sleep(cfg_.reconnect_delay);
      continue;
    }

    if (headers.find("200") == std::string::npos) {
      SetError("HTTP error: " + headers.substr(0, 80));
      ::close(sock_);
      sock_ = -1;
      Sleep(cfg_.reconnect_delay);
      continue;
    }

    while (running_.load()) {
      auto frame = ReadGrpcFrame();
      if (frame.empty()) {
        break;
      }
      std::string json_str(frame.begin(), frame.end());
      JsonValue val = Json::ParseSafe(json_str);
      EmitData(val);
    }

    ::close(sock_);
    sock_ = -1;
    if (running_.load()) {
      Sleep(cfg_.reconnect_delay);
    }
  }
}

// ── DistributedDashboard
// ──────────────────────────────────────────────────────

DistributedDashboard& DistributedDashboard::AddSource(
    const std::string& title,
    std::shared_ptr<LiveSource<std::string>> src) {
  Entry e;
  e.title = title;
  e.string_src = std::move(src);
  e.is_json = false;
  entries_.push_back(std::move(e));
  return *this;
}

DistributedDashboard& DistributedDashboard::AddJsonSource(
    const std::string& title,
    std::shared_ptr<LiveSource<JsonValue>> src) {
  Entry e;
  e.title = title;
  e.json_src = std::move(src);
  e.is_json = true;
  entries_.push_back(std::move(e));
  return *this;
}

DistributedDashboard& DistributedDashboard::WithLayout(int cols) {
  cols_ = cols;
  return *this;
}

ftxui::Component DistributedDashboard::Build() const {
  using namespace ftxui;

  struct SharedState {
    std::vector<std::string> latest_values;
    std::vector<bool> connected;
    std::vector<std::string> last_updated;
    std::mutex mtx;
  };
  auto state = std::make_shared<SharedState>();
  state->latest_values.resize(entries_.size(), "(no data)");
  state->connected.resize(entries_.size(), false);
  state->last_updated.resize(entries_.size(), "never");

  for (size_t i = 0; i < entries_.size(); i++) {
    const auto& entry = entries_[i];
    if (!entry.is_json && entry.string_src) {
      entry.string_src->OnData([state, i](const std::string& v) {
        std::lock_guard<std::mutex> lock(state->mtx);
        state->latest_values[i] = v;
        state->connected[i] = true;
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
        state->last_updated[i] = buf;
      });
      if (!entry.string_src->IsRunning()) {
        entry.string_src->Start();
      }
    } else if (entry.is_json && entry.json_src) {
      entry.json_src->OnData([state, i](const JsonValue& v) {
        std::lock_guard<std::mutex> lock(state->mtx);
        state->latest_values[i] = v.is_null()     ? "null"
                                  : v.is_string() ? v.as_string()
                                                  : "(json)";
        state->connected[i] = true;
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
        state->last_updated[i] = buf;
      });
      if (!entry.json_src->IsRunning()) {
        entry.json_src->Start();
      }
    }
  }

  auto entries_copy = entries_;
  int cols = cols_;

  return Renderer([state, entries_copy, cols] {
    std::vector<Element> rows;
    std::vector<Element> row;

    for (size_t i = 0; i < entries_copy.size(); i++) {
      std::string val, updated, title;
      bool conn = false;
      {
        std::lock_guard<std::mutex> lock(state->mtx);
        val = state->latest_values[i];
        conn = state->connected[i];
        updated = state->last_updated[i];
        title = entries_copy[i].title;
      }

      Color border_color = conn ? Color::Green : Color::Red;
      auto cell =
          window(text(title) | bold,
                 vbox({
                     text(val) | flex,
                     hbox({text(conn ? " ● Connected" : " ○ Disconnected") |
                               color(border_color),
                           filler(), text(updated)}),
                 })) |
          color(border_color);

      row.push_back(cell | flex);
      if (static_cast<int>(row.size()) >= cols ||
          i + 1 == entries_copy.size()) {
        while (static_cast<int>(row.size()) < cols) {
          row.push_back(filler());
        }
        rows.push_back(hbox(row));
        row.clear();
      }
    }

    if (rows.empty()) {
      return text("No sources configured") | center;
    }

    return vbox(rows);
  });
}

}  // namespace ftxui::ui
