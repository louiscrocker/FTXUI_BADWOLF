// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_LIVE_SOURCE_HPP
#define FTXUI_UI_LIVE_SOURCE_HPP

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/ui/reactive.hpp"

namespace ftxui::ui {

// Base live data source — polls T on a background thread.
// Template implementations are fully in this header.
template <typename T>
class LiveSource {
 public:
  using Callback = std::function<void(T)>;

  virtual ~LiveSource() { Stop(); }

  // Start background polling thread (idempotent).
  void Start() {
    if (running_.exchange(true)) {
      return;
    }
    thread_ = std::thread([this] {
      while (running_.load()) {
        T val{};
        try {
          val = Fetch();
        } catch (...) {
          // Errors silently ignored; returns default value.
        }
        {
          std::lock_guard<std::mutex> lock(mutex_);
          latest_ = val;
        }
        // Snapshot and fire callbacks outside the lock.
        std::map<int, Callback> cbs;
        {
          std::lock_guard<std::mutex> lock(mutex_);
          cbs = callbacks_;
        }
        for (auto& [id, cb] : cbs) {
          cb(val);
        }
        // Request UI redraw.
        if (App* app = App::Active()) {
          app->PostEvent(Event::Custom);
        }
        // Interruptible sleep for Interval().
        std::unique_lock<std::mutex> lock(cv_mutex_);
        cv_.wait_for(lock, Interval(), [this] { return !running_.load(); });
      }
    });
  }

  // Signal background thread to stop and join it.
  void Stop() {
    running_.store(false);
    cv_.notify_all();
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  bool IsRunning() const { return running_.load(); }

  // Subscribe to new data. Returns handle id. Called on background thread.
  int OnData(Callback cb) {
    std::lock_guard<std::mutex> lock(mutex_);
    int id = next_id_++;
    callbacks_[id] = std::move(cb);
    return id;
  }

  void RemoveOnData(int id) {
    std::lock_guard<std::mutex> lock(mutex_);
    callbacks_.erase(id);
  }

  // Returns the most recently fetched value.
  T Latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
  }

 protected:
  virtual T Fetch() = 0;
  virtual std::chrono::milliseconds Interval() const {
    return std::chrono::milliseconds(1000);
  }

 private:
  std::thread thread_;
  std::atomic<bool> running_{false};
  T latest_{};
  mutable std::mutex mutex_;
  std::mutex cv_mutex_;
  std::condition_variable cv_;
  std::map<int, Callback> callbacks_;
  int next_id_{0};
};

// ── HTTP JSON polling source ───────────────────────────────────────────────

// Polls an HTTP endpoint via raw POSIX TCP socket and returns the response
// body.
class HttpJsonSource : public LiveSource<std::string> {
 public:
  HttpJsonSource(std::string host,
                 int port,
                 std::string path,
                 std::chrono::milliseconds interval = std::chrono::seconds(5));

 protected:
  std::string Fetch() override;
  std::chrono::milliseconds Interval() const override { return interval_; }

 private:
  std::string host_;
  int port_;
  std::string path_;
  std::chrono::milliseconds interval_;
};

// ── Prometheus metrics source ──────────────────────────────────────────────

struct PrometheusMetric {
  std::string name;
  std::string help;
  std::string type;
  std::vector<std::pair<std::string, double>> samples;  // {label, value}
};

// Scrapes a Prometheus /metrics endpoint and parses the text format.
class PrometheusSource : public LiveSource<std::vector<PrometheusMetric>> {
 public:
  explicit PrometheusSource(std::string host = "localhost",
                            int port = 9090,
                            std::string path = "/metrics");

 protected:
  std::vector<PrometheusMetric> Fetch() override;

 private:
  std::string host_;
  std::string path_;
  int port_;

 public:
  // Exposed for testing.
  static std::vector<PrometheusMetric> ParsePrometheusText(
      const std::string& text);
};

// ── File tail source ───────────────────────────────────────────────────────

// Watches a file for new lines (like `tail -f`).
class FileTailSource : public LiveSource<std::string> {
 public:
  explicit FileTailSource(std::string filepath, int max_lines = 1000);

 protected:
  std::string Fetch() override;
  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(500);
  }

 private:
  std::string filepath_;
  std::streampos last_pos_{0};
  int max_lines_;
};

// ── Stdin/pipe source ──────────────────────────────────────────────────────

// Non-blocking stdin reader using select().
class StdinSource : public LiveSource<std::string> {
 public:
  StdinSource();

 protected:
  std::string Fetch() override;
  std::chrono::milliseconds Interval() const override {
    return std::chrono::milliseconds(100);
  }
};

// ── Reactive binding ───────────────────────────────────────────────────────

// Connect LiveSource<T> → Reactive<T>. Starts source automatically.
template <typename T>
std::shared_ptr<Reactive<T>> BindLiveSource(
    std::shared_ptr<LiveSource<T>> source) {
  auto reactive = std::make_shared<Reactive<T>>();
  source->OnData([reactive](T val) { reactive->Set(std::move(val)); });
  if (!source->IsRunning()) {
    source->Start();
  }
  return reactive;
}

// ── Widget factories ───────────────────────────────────────────────────────

// LogPanel auto-connected to a FileTailSource.
Component LiveLogPanel(std::shared_ptr<FileTailSource> source,
                       int max_lines = 100);

// Metrics table auto-connected to a PrometheusSource.
Component LiveMetricsTable(std::shared_ptr<PrometheusSource> source);

// Line chart (sparkline ring buffer) auto-connected to LiveSource<double>.
Component LiveLineChart(std::shared_ptr<LiveSource<double>> source,
                        const std::string& title = "",
                        int history = 60);

// JSON text viewer auto-connected to HttpJsonSource.
Component LiveJsonViewer(std::shared_ptr<HttpJsonSource> source);

}  // namespace ftxui::ui

#endif  // FTXUI_UI_LIVE_SOURCE_HPP
