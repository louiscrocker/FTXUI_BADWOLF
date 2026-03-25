// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/llm_bridge.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/dom/table.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/charts.hpp"
#include "ftxui/ui/form.hpp"
#include "ftxui/ui/geojson.hpp"
#include "ftxui/ui/geomap.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/progress.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/world_map_data.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── String helpers
// ────────────────────────────────────────────────────────────

namespace {

std::string ToLower(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

bool ContainsWord(const std::string& haystack, const std::string& needle) {
  size_t pos = haystack.find(needle);
  while (pos != std::string::npos) {
    bool left_ok = (pos == 0) ||
                   !std::isalnum(static_cast<unsigned char>(haystack[pos - 1]));
    bool right_ok = (pos + needle.size() >= haystack.size()) ||
                    !std::isalnum(static_cast<unsigned char>(
                        haystack[pos + needle.size()]));
    if (left_ok && right_ok) {
      return true;
    }
    pos = haystack.find(needle, pos + 1);
  }
  return false;
}

// Splits on whitespace/punctuation into tokens.
std::vector<std::string> Tokenize(const std::string& s) {
  std::vector<std::string> tokens;
  std::string cur;
  for (char c : s) {
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
      cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    } else {
      if (!cur.empty()) {
        tokens.push_back(cur);
        cur.clear();
      }
    }
  }
  if (!cur.empty()) {
    tokens.push_back(cur);
  }
  return tokens;
}

// Collect comma/and-separated tokens after a keyword index.
std::vector<std::string> ExtractListAfter(
    const std::vector<std::string>& tokens,
    size_t start) {
  std::vector<std::string> result;
  for (size_t i = start; i < tokens.size(); ++i) {
    if (tokens[i] == "and" || tokens[i] == "or") {
      continue;
    }
    // Stop at structural keywords
    static const std::array<std::string, 6> stop_words = {
        "in", "with", "at", "size", "color", "border"};
    bool is_stop = false;
    for (const auto& sw : stop_words) {
      if (tokens[i] == sw) {
        is_stop = true;
        break;
      }
    }
    if (is_stop) {
      break;
    }
    result.push_back(tokens[i]);
  }
  return result;
}

// ── Sample data sets
// ──────────────────────────────────────────────────────────

using TableData = std::vector<std::vector<std::string>>;

struct SampleDataset {
  std::vector<std::string> headers;
  TableData rows;
};

SampleDataset SampleFor(const std::string& entity) {
  if (entity == "users" || entity == "user") {
    return {{"Name", "Email", "Role", "Status"},
            {{"Alice", "alice@example.com", "Admin", "Active"},
             {"Bob", "bob@example.com", "Editor", "Active"},
             {"Carol", "carol@example.com", "Viewer", "Inactive"},
             {"Dave", "dave@example.com", "Admin", "Active"},
             {"Eve", "eve@example.com", "Editor", "Active"}}};
  }
  if (entity == "servers" || entity == "server") {
    return {{"Host", "CPU %", "Mem %", "Status"},
            {{"web-01", "42", "67", "OK"},
             {"web-02", "88", "54", "Warn"},
             {"db-01", "23", "91", "Crit"},
             {"cache-01", "12", "34", "OK"},
             {"worker-01", "56", "72", "OK"}}};
  }
  if (entity == "metrics" || entity == "metric") {
    return {{"Metric", "Value", "Unit", "Trend"},
            {{"Requests/s", "1245", "req/s", "↑"},
             {"Latency p99", "142", "ms", "→"},
             {"Error Rate", "0.4", "%", "↓"},
             {"Cache Hit", "87", "%", "↑"},
             {"Queue Depth", "23", "items", "↑"}}};
  }
  if (entity == "files" || entity == "file") {
    return {{"Name", "Size", "Type", "Modified"},
            {{"README.md", "4.2 KB", "Markdown", "2024-01-15"},
             {"main.cpp", "12.8 KB", "C++ Source", "2024-01-14"},
             {"CMakeLists.txt", "3.1 KB", "CMake", "2024-01-13"},
             {"LICENSE", "1.1 KB", "Text", "2024-01-01"},
             {"Makefile", "2.7 KB", "Makefile", "2024-01-10"}}};
  }
  if (entity == "logs" || entity == "log") {
    return {{"Time", "Level", "Message"},
            {{"10:01:05", "INFO", "Server started on port 8080"},
             {"10:01:07", "DEBUG", "Worker pool initialized (8 threads)"},
             {"10:02:14", "WARN", "High memory pressure (87 %)"},
             {"10:03:22", "ERROR", "Connection pool exhausted"},
             {"10:03:25", "INFO", "Recovered: pool resized to 32"}}};
  }
  // Generic fallback
  SampleDataset ds;
  ds.headers = {"Name", "Value"};
  std::string cap = entity;
  if (!cap.empty()) {
    cap[0] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(cap[0])));
  }
  for (int i = 1; i <= 5; ++i) {
    ds.rows.push_back({cap + " " + std::to_string(i), std::to_string(i * 10)});
  }
  return ds;
}

// Build a DOM table Element from headers + rows.
Element MakeTableElement(const SampleDataset& ds) {
  const Theme& t = GetTheme();
  std::vector<std::vector<Element>> cells;

  // Header row
  std::vector<Element> header_row;
  for (const auto& h : ds.headers) {
    header_row.push_back(text(h) | bold | color(t.primary) |
                         size(WIDTH, EQUAL, static_cast<int>(h.size()) + 2) |
                         hcenter);
  }
  cells.push_back(std::move(header_row));

  // Data rows
  for (const auto& row : ds.rows) {
    std::vector<Element> elems;
    for (size_t col = 0; col < ds.headers.size(); ++col) {
      std::string val = (col < row.size()) ? row[col] : "";
      int w =
          static_cast<int>(std::max(val.size(), ds.headers[col].size()) + 2);
      elems.push_back(text(val) | size(WIDTH, EQUAL, w));
    }
    cells.push_back(std::move(elems));
  }

  auto tbl = ftxui::Table(std::move(cells));
  tbl.SelectAll().Border(LIGHT);
  tbl.SelectRow(0).Border(LIGHT);
  tbl.SelectRow(0).DecorateCells(bold);
  tbl.SelectColumns(0, -1).SeparatorVertical(LIGHT);

  return tbl.Render();
}

// Build sinusoidal data for a LineChart.
Component MakeChartComponent(const UIIntent& intent) {
  std::string title = intent.entity.empty() ? "Data" : intent.entity;

  LineChart chart;
  const int N = 60;
  std::vector<float> v1(N), v2(N);
  for (int i = 0; i < N; ++i) {
    v1[i] = 50.f + 30.f * std::sin(static_cast<float>(i) * 0.2f);
    v2[i] = 50.f + 20.f * std::cos(static_cast<float>(i) * 0.15f + 1.f);
  }
  chart.Title(title)
      .Series("Series A", v1, Color::Cyan)
      .Series("Series B", v2, Color::Yellow);
  chart.ShowLegend(true).ShowAxes(true);
  return chart.Build();
}

// Build a Form component for the given intent.
Component MakeFormComponent(const UIIntent& intent) {
  auto form = Form();
  if (!intent.entity.empty()) {
    std::string cap = intent.entity;
    cap[0] =
        static_cast<char>(std::toupper(static_cast<unsigned char>(cap[0])));
    form.Title(cap + " Form");
  }

  // Use explicit fields if provided, otherwise fall back to entity defaults.
  if (!intent.fields.empty()) {
    for (const auto& field : intent.fields) {
      std::string label = field;
      if (!label.empty()) {
        label[0] = static_cast<char>(
            std::toupper(static_cast<unsigned char>(label[0])));
      }
      form.Field(label, nullptr);
    }
  } else if (intent.entity == "users" || intent.entity == "user" ||
             intent.entity == "registration") {
    static std::string name, email, password;
    form.Field("Name", &name, "Alice")
        .Field("Email", &email, "you@example.com")
        .Password("Password", &password);
  } else if (intent.entity == "servers" || intent.entity == "server") {
    static std::string host, port, user;
    form.Field("Host", &host, "192.168.1.1")
        .Field("Port", &port, "22")
        .Field("User", &user, "root");
  } else {
    static std::string name, value, notes;
    form.Field("Name", &name, "")
        .Field("Value", &value, "")
        .Field("Notes", &notes, "");
  }
  form.Submit("  Submit  ", [] {});
  return form.Build();
}

// Build a DASHBOARD (2x2 grid of mixed widgets).
Component MakeDashboardComponent(const UIIntent& intent) {
  auto make_box = [](const std::string& title, Element content) -> Element {
    const Theme& t = GetTheme();
    return window(text(" " + title + " ") | bold | color(t.primary),
                  content | flex) |
           flex;
  };

  // Top-left: mini table
  auto ds = SampleFor("servers");
  auto tbl_elem = MakeTableElement(ds);

  // Top-right: sparkline chart
  const int N = 40;
  std::vector<float> vals(N);
  for (int i = 0; i < N; ++i) {
    vals[i] = 50.f + 30.f * std::sin(static_cast<float>(i) * 0.3f);
  }

  // Bottom-left: metrics
  auto metrics_ds = SampleFor("metrics");
  auto metrics_elem = MakeTableElement(metrics_ds);

  // Bottom-right: progress bars
  static float p1 = 0.42f, p2 = 0.77f, p3 = 0.23f;
  auto prog_elem = vbox({
      hbox({text("CPU  ") | size(WIDTH, EQUAL, 6), gauge(p1) | flex,
            text(" 42%")}),
      hbox({text("Mem  ") | size(WIDTH, EQUAL, 6), gauge(p2) | flex,
            text(" 77%")}),
      hbox({text("Disk ") | size(WIDTH, EQUAL, 6), gauge(p3) | flex,
            text(" 23%")}),
  });

  std::string title = intent.entity.empty() ? "Dashboard" : intent.entity;
  (void)title;

  return Renderer([=]() -> Element {
    return vbox({
        hbox({
            make_box("Servers", tbl_elem),
            make_box("Trend", vbox({text("")})),
        }) | flex,
        hbox({
            make_box("Metrics", metrics_elem),
            make_box("Resources", prog_elem),
        }) | flex,
    });
  });
}

// Build a GeoMap component.
Component MakeMapComponent() {
  try {
    auto json_str = WorldMapGeoJSON();
    auto collection = ParseGeoJSON(json_str);
    return GeoMap().Data(collection).Build();
  } catch (...) {
    return Renderer([] {
      return text("Map unavailable — world_map_data not loaded") |
             color(Color::Red);
    });
  }
}

// Build a LogPanel component with sample entries.
Component MakeLogComponent(const UIIntent& intent) {
  auto log = LogPanel::Create(500);

  // Seed with sample entries
  log->Info("Service started");
  log->Debug("Config loaded: max_conn=100, timeout=30s");
  log->Info("Listening on 0.0.0.0:8080");
  log->Warn("Connection pool at 80 % capacity");
  log->Error("Failed to reach upstream: connection refused");
  log->Info("Upstream reconnected after 3.2 s");

  if (intent.entity == "servers" || intent.entity == "server") {
    log->Debug("Heartbeat OK — web-01, web-02, db-01");
    log->Warn("db-01: disk usage at 91 %");
  } else if (intent.entity == "metrics" || intent.entity == "metric") {
    log->Info("Metrics flush: p50=42ms p99=142ms");
    log->Debug("Cache hit ratio: 87.3 %");
  }

  std::string panel_title =
      intent.entity.empty() ? "Application Log" : intent.entity + " log";
  return log->Build(panel_title);
}

}  // namespace

// ── NLParser
// ──────────────────────────────────────────────────────────────────

UIIntent NLParser::Parse(const std::string& natural_language) {
  UIIntent intent;
  intent.raw_input = natural_language;

  if (natural_language.empty()) {
    return intent;
  }

  const std::string low = ToLower(natural_language);
  const auto tokens = Tokenize(low);

  // ── Determine IntentType by keyword priority
  auto has = [&](const std::string& w) { return ContainsWord(low, w); };

  if (has("chart") || has("graph") || has("plot") || has("histogram") ||
      has("sparkline")) {
    intent.type = IntentType::CHART;
  } else if (has("dashboard") || has("overview") || has("monitor")) {
    intent.type = IntentType::DASHBOARD;
  } else if (has("map") || has("world") || has("geo")) {
    intent.type = IntentType::MAP;
  } else if (has("log") || has("events") || has("history")) {
    intent.type = IntentType::LOG;
  } else if (has("progress") || has("loading") || has("status")) {
    intent.type = IntentType::PROGRESS;
  } else if (has("form") || has("input") || has("create") || has("edit") ||
             has("add")) {
    intent.type = IntentType::FORM;
  } else if (has("table") || has("list") || has("show") || has("display") ||
             has("rows") || has("columns")) {
    intent.type = IntentType::TABLE;
  } else {
    intent.type = IntentType::UNKNOWN;
  }

  // ── Extract entity: word that follows a trigger verb/preposition
  static const std::array<std::string, 12> trigger_words = {
      "show",    "display", "plot", "of",  "for",   "list",
      "monitor", "create",  "edit", "add", "table", "log"};
  static const std::array<std::string, 10> skip_words = {
      "a", "an", "the", "me", "my", "all", "some", "new", "of", "for"};
  // These are type keywords, not entities — skip them when extracting entity
  static const std::array<std::string, 8> type_words = {
      "list", "table", "chart", "graph", "form", "log", "map", "dashboard"};

  for (size_t i = 0; i < tokens.size(); ++i) {
    bool is_trigger = false;
    for (const auto& t : trigger_words) {
      if (tokens[i] == t) {
        is_trigger = true;
        break;
      }
    }
    if (!is_trigger) {
      continue;
    }
    // Collect subsequent non-stop tokens as entity
    std::string entity;
    for (size_t j = i + 1; j < tokens.size(); ++j) {
      bool skip = false;
      for (const auto& sw : skip_words) {
        if (tokens[j] == sw) {
          skip = true;
          break;
        }
      }
      // Also skip type keywords (list, table, chart) — they describe the
      // widget, not the entity
      if (!skip) {
        for (const auto& tw : type_words) {
          if (tokens[j] == tw) {
            skip = true;
            break;
          }
        }
      }
      if (skip) {
        continue;
      }
      // Stop at structural keywords
      if (tokens[j] == "with" || tokens[j] == "in" || tokens[j] == "at" ||
          tokens[j] == "using" || tokens[j] == "and" ||
          tokens[j] == "columns" || tokens[j] == "fields") {
        break;
      }
      if (!entity.empty()) {
        entity += "_";
      }
      entity += tokens[j];
      // Only take the first meaningful word
      break;
    }
    if (!entity.empty()) {
      intent.entity = entity;
      break;
    }
  }

  // ── Extract fields: "with columns name, age, email"
  //                    "with fields name and email"
  for (size_t i = 0; i < tokens.size(); ++i) {
    if ((tokens[i] == "columns" || tokens[i] == "fields") &&
        i + 1 < tokens.size()) {
      auto field_tokens = ExtractListAfter(tokens, i + 1);
      for (const auto& f : field_tokens) {
        if (f != "and" && f != "or" && f != "with") {
          intent.fields.push_back(f);
        }
      }
      break;
    }
  }

  // ── Extract options
  // color
  static const std::array<std::string, 8> colors = {
      "blue", "green", "red", "yellow", "cyan", "white", "orange", "purple"};
  for (const auto& c : colors) {
    if (has(c)) {
      intent.options["color"] = c;
      break;
    }
  }
  // size
  if (has("large") || has("big")) {
    intent.options["size"] = "large";
  } else if (has("small") || has("compact")) {
    intent.options["size"] = "small";
  }
  // border
  if (has("border") || has("bordered")) {
    intent.options["border"] = "true";
  }

  return intent;
}

std::vector<UIIntent> NLParser::ParseMulti(const std::string& nl) {
  // Split on "and", "also", "plus", "then" at the sentence level
  static const std::array<std::string, 4> splitters = {" and ", " also ",
                                                       " plus ", " then "};
  std::vector<std::string> parts = {nl};
  for (const auto& sp : splitters) {
    std::vector<std::string> new_parts;
    for (const auto& part : parts) {
      std::string low = ToLower(part);
      size_t pos = low.find(sp);
      if (pos != std::string::npos) {
        new_parts.push_back(part.substr(0, pos));
        new_parts.push_back(part.substr(pos + sp.size()));
      } else {
        new_parts.push_back(part);
      }
    }
    parts = std::move(new_parts);
  }

  std::vector<UIIntent> intents;
  intents.reserve(parts.size());
  for (const auto& p : parts) {
    if (!p.empty()) {
      intents.push_back(NLParser::Parse(p));
    }
  }
  return intents;
}

// ── UIGenerator
// ───────────────────────────────────────────────────────────────

Component UIGenerator::Generate(const UIIntent& intent) {
  switch (intent.type) {
    case IntentType::TABLE: {
      auto ds = SampleFor(intent.entity);
      // Override headers if explicit fields provided
      if (!intent.fields.empty()) {
        ds.headers.clear();
        for (const auto& f : intent.fields) {
          std::string h = f;
          if (!h.empty()) {
            h[0] = static_cast<char>(
                std::toupper(static_cast<unsigned char>(h[0])));
          }
          ds.headers.push_back(h);
        }
        // Trim rows to new column count
        for (auto& row : ds.rows) {
          row.resize(ds.headers.size());
        }
      }
      return Renderer([ds]() { return MakeTableElement(ds); });
    }

    case IntentType::CHART:
      return MakeChartComponent(intent);

    case IntentType::FORM:
      return MakeFormComponent(intent);

    case IntentType::DASHBOARD:
      return MakeDashboardComponent(intent);

    case IntentType::MAP:
      return MakeMapComponent();

    case IntentType::LOG:
      return MakeLogComponent(intent);

    case IntentType::PROGRESS: {
      static float prog = 0.67f;
      std::string lbl = intent.entity.empty() ? "Loading" : intent.entity;
      return ThemedProgressBar(&prog, lbl);
    }

    case IntentType::UNKNOWN:
    default: {
      std::string msg = intent.raw_input;
      return Renderer([msg]() -> Element {
        return hbox({
            text("I don't understand: ") | bold,
            text(msg) | color(Color::Red),
        });
      });
    }
  }
}

Component UIGenerator::GenerateFromText(const std::string& nl) {
  return Generate(NLParser::Parse(nl));
}

Element UIGenerator::PreviewElement(const UIIntent& intent) {
  switch (intent.type) {
    case IntentType::TABLE: {
      auto ds = SampleFor(intent.entity);
      return MakeTableElement(ds);
    }
    case IntentType::CHART: {
      std::string title = intent.entity.empty() ? "Chart" : intent.entity;
      return text("[LineChart: " + title + "]") | bold;
    }
    case IntentType::FORM: {
      std::string title = intent.entity.empty() ? "Form" : intent.entity;
      return text("[Form: " + title + "]") | bold;
    }
    case IntentType::DASHBOARD:
      return text("[Dashboard]") | bold;
    case IntentType::MAP:
      return text("[World Map]") | bold;
    case IntentType::LOG: {
      std::string title = intent.entity.empty() ? "Log" : intent.entity;
      return text("[Log Panel: " + title + "]") | bold;
    }
    case IntentType::PROGRESS: {
      static float p = 0.67f;
      return gauge(p) | color(Color::Cyan);
    }
    case IntentType::UNKNOWN:
    default:
      return hbox({text("Unknown: ") | bold,
                   text(intent.raw_input) | color(Color::Red)});
  }
}

// ── LLMAdapter — HTTP via raw POSIX sockets
// ───────────────────────────────────────────

namespace {

// Non-blocking connect with a 1-second timeout.
bool TryConnect(int fd, const struct sockaddr_in& addr) {
  int flags = ::fcntl(fd, F_GETFL, 0);
  if (flags < 0) {
    return false;
  }
  ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  int r = ::connect(fd, reinterpret_cast<const struct sockaddr*>(&addr),
                    sizeof(addr));
  if (r == 0) {
    // Immediate success (unusual for local socket but handle it)
    ::fcntl(fd, F_SETFL, flags);  // restore blocking
    return true;
  }
  if (errno != EINPROGRESS) {
    return false;
  }

  fd_set wfds;
  FD_ZERO(&wfds);
  FD_SET(fd, &wfds);
  struct timeval tv = {1, 0};
  int sel = ::select(fd + 1, nullptr, &wfds, nullptr, &tv);
  if (sel <= 0) {
    return false;
  }
  int error = 0;
  socklen_t len = sizeof(error);
  ::getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
  if (error != 0) {
    return false;
  }

  ::fcntl(fd, F_SETFL, flags);  // restore blocking mode
  return true;
}

// Send all bytes on a blocking socket.
bool SendAll(int fd, const std::string& data) {
  ssize_t sent = 0;
  while (sent < static_cast<ssize_t>(data.size())) {
    ssize_t n = ::send(fd, data.data() + sent,
                       data.size() - static_cast<size_t>(sent), 0);
    if (n <= 0) {
      return false;
    }
    sent += n;
  }
  return true;
}

// Receive until connection closes or we have the full HTTP body.
std::string RecvAll(int fd) {
  std::string buf;
  char tmp[4096];
  while (true) {
    ssize_t n = ::recv(fd, tmp, sizeof(tmp), 0);
    if (n <= 0) {
      break;
    }
    buf.append(tmp, static_cast<size_t>(n));
  }
  return buf;
}

// Extract the HTTP body (after \r\n\r\n).
std::string HttpBody(const std::string& response) {
  auto pos = response.find("\r\n\r\n");
  if (pos == std::string::npos) {
    return response;
  }
  return response.substr(pos + 4);
}

// Minimal JSON string-value extractor: find "key":"value" (no nested objects).
std::string JsonStringField(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\"";
  auto kpos = json.find(search);
  if (kpos == std::string::npos) {
    return "";
  }
  auto colon = json.find(':', kpos + search.size());
  if (colon == std::string::npos) {
    return "";
  }
  // Skip whitespace
  size_t start = colon + 1;
  while (start < json.size() && json[start] != '"') {
    ++start;
  }
  if (start >= json.size()) {
    return "";
  }
  ++start;  // skip opening quote
  std::string val;
  while (start < json.size() && json[start] != '"') {
    if (json[start] == '\\' && start + 1 < json.size()) {
      char esc = json[start + 1];
      if (esc == '"') {
        val += '"';
      } else if (esc == 'n') {
        val += '\n';
      } else if (esc == '\\') {
        val += '\\';
      } else {
        val += esc;
      }
      start += 2;
    } else {
      val += json[start];
      ++start;
    }
  }
  return val;
}

// Parse a JSON array of strings: ["a","b","c"]
std::vector<std::string> JsonStringArray(const std::string& json,
                                         const std::string& key) {
  std::string search = "\"" + key + "\"";
  auto kpos = json.find(search);
  if (kpos == std::string::npos) {
    return {};
  }
  auto bracket = json.find('[', kpos);
  if (bracket == std::string::npos) {
    return {};
  }
  auto end_bracket = json.find(']', bracket);
  if (end_bracket == std::string::npos) {
    return {};
  }
  std::string arr_str = json.substr(bracket + 1, end_bracket - bracket - 1);
  std::vector<std::string> result;
  size_t pos = 0;
  while (pos < arr_str.size()) {
    auto q1 = arr_str.find('"', pos);
    if (q1 == std::string::npos) {
      break;
    }
    auto q2 = arr_str.find('"', q1 + 1);
    if (q2 == std::string::npos) {
      break;
    }
    result.push_back(arr_str.substr(q1 + 1, q2 - q1 - 1));
    pos = q2 + 1;
  }
  return result;
}

// Parse a JSON intent object into UIIntent.
UIIntent ParseJsonIntent(const std::string& json,
                         const std::string& raw_input) {
  UIIntent intent;
  intent.raw_input = raw_input;

  std::string type_str = ToLower(JsonStringField(json, "type"));
  if (type_str == "table") {
    intent.type = IntentType::TABLE;
  } else if (type_str == "chart") {
    intent.type = IntentType::CHART;
  } else if (type_str == "form") {
    intent.type = IntentType::FORM;
  } else if (type_str == "dashboard") {
    intent.type = IntentType::DASHBOARD;
  } else if (type_str == "map") {
    intent.type = IntentType::MAP;
  } else if (type_str == "log") {
    intent.type = IntentType::LOG;
  } else if (type_str == "progress") {
    intent.type = IntentType::PROGRESS;
  } else {
    intent.type = IntentType::UNKNOWN;
  }

  intent.entity = JsonStringField(json, "entity");
  intent.fields = JsonStringArray(json, "fields");

  // options: simple key-value extraction
  std::string color = JsonStringField(json, "color");
  if (!color.empty()) {
    intent.options["color"] = color;
  }
  std::string size = JsonStringField(json, "size");
  if (!size.empty()) {
    intent.options["size"] = size;
  }

  // If parsing failed, fall back to rule-based
  if (intent.type == IntentType::UNKNOWN && intent.entity.empty()) {
    return NLParser::Parse(raw_input);
  }
  return intent;
}

// Simple JSON escaping for the prompt string.
std::string JsonEscape(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (char c : s) {
    if (c == '"') {
      out += "\\\"";
    } else if (c == '\\') {
      out += "\\\\";
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else {
      out += c;
    }
  }
  return out;
}

}  // namespace

bool LLMAdapter::OllamaAvailable() {
  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return false;
  }
  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(11434);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  bool ok = TryConnect(fd, addr);
  ::close(fd);
  return ok;
}

bool LLMAdapter::OpenAIAvailable() {
  return std::getenv("OPENAI_API_KEY") != nullptr;
}

std::string LLMAdapter::SystemPrompt() {
  return "You are a UI generator assistant. The user will describe a terminal "
         "UI "
         "component in plain English. You must respond with ONLY valid JSON in "
         "this exact format (no markdown, no explanation):\n"
         "{\"type\":\"TABLE\",\"entity\":\"users\","
         "\"fields\":[\"name\",\"email\"],\"options\":{}}\n\n"
         "Valid types: TABLE, CHART, FORM, DASHBOARD, MAP, LOG, PROGRESS, "
         "UNKNOWN.\n"
         "Set entity to the data subject (e.g. \"users\", \"servers\").\n"
         "Set fields to column names if mentioned, otherwise [].\n"
         "Set options to {} unless color/size/border is specified.\n"
         "Respond ONLY with the JSON object.";
}

UIIntent LLMAdapter::QueryLLM(const std::string& prompt,
                              const std::string& model) {
  if (!OllamaAvailable()) {
    return NLParser::Parse(prompt);
  }

  int fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return NLParser::Parse(prompt);
  }

  struct sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(11434);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (!TryConnect(fd, addr)) {
    ::close(fd);
    return NLParser::Parse(prompt);
  }

  // Build JSON body
  std::string system_prompt = JsonEscape(SystemPrompt());
  std::string user_prompt = JsonEscape(prompt);
  std::string body = "{\"model\":\"" + model + "\",\"prompt\":\"" +
                     user_prompt + "\",\"system\":\"" + system_prompt +
                     "\",\"stream\":false}";

  // Build HTTP request
  std::string request =
      "POST /api/generate HTTP/1.0\r\n"
      "Host: localhost:11434\r\n"
      "Content-Type: application/json\r\n"
      "Content-Length: " +
      std::to_string(body.size()) +
      "\r\n"
      "Connection: close\r\n"
      "\r\n" +
      body;

  if (!SendAll(fd, request)) {
    ::close(fd);
    return NLParser::Parse(prompt);
  }

  std::string raw = RecvAll(fd);
  ::close(fd);

  std::string body_str = HttpBody(raw);
  // The "response" field contains the LLM-generated JSON intent
  std::string llm_response = JsonStringField(body_str, "response");
  if (llm_response.empty()) {
    return NLParser::Parse(prompt);
  }

  return ParseJsonIntent(llm_response, prompt);
}

// ── LLMRepl
// ───────────────────────────────────────────────────────────────────

namespace {

struct ReplState {
  std::string input;
  std::string last_prompt;
  ftxui::Component current_component;
  std::vector<std::string> history;
  bool show_help = false;

  std::string backend_label;
};

const std::array<std::string, 5> kExamplePrompts = {
    "show me a table of users",
    "display a chart of server metrics",
    "create a form for user registration",
    "show world map",
    "monitor server logs",
};

}  // namespace

Component LLMRepl() {
  auto state = std::make_shared<ReplState>();

  // Determine active backend
  if (LLMAdapter::OllamaAvailable()) {
    state->backend_label = "Ollama: llama3";
  } else if (LLMAdapter::OpenAIAvailable()) {
    state->backend_label = "OpenAI: gpt-4 (fallback to NLP)";
  } else {
    state->backend_label = "Rule-based NLP";
  }

  // Start with a default component
  state->current_component =
      UIGenerator::GenerateFromText("show me a table of users");
  state->last_prompt = "show me a table of users";

  InputOption input_opt;
  input_opt.on_enter = [state]() {
    if (state->input.empty()) {
      return;
    }
    std::string prompt = state->input;
    state->input.clear();

    // Keep last 10 history entries
    state->history.push_back(prompt);
    if (state->history.size() > 10) {
      state->history.erase(state->history.begin());
    }

    // Generate component (use LLM if available)
    UIIntent intent = LLMAdapter::QueryLLM(prompt);
    state->current_component = UIGenerator::Generate(intent);
    state->last_prompt = prompt;
  };

  auto text_input =
      Input(&state->input, "Describe a UI component...", input_opt);

  auto help_overlay = Renderer([state]() -> Element {
    const Theme& t = GetTheme();
    std::vector<Element> lines;
    lines.push_back(text(" Example prompts:") | bold | color(t.primary));
    lines.push_back(separatorEmpty());
    for (const auto& ex : kExamplePrompts) {
      lines.push_back(hbox({text("  • "), text(ex) | color(t.secondary)}));
    }
    lines.push_back(separatorEmpty());
    lines.push_back(text(" Press Ctrl+H to close") | color(t.text_muted));
    return window(text(" Help ") | bold | color(t.primary),
                  vbox(std::move(lines)));
  });

  auto root =
      Renderer(text_input, [state, text_input, help_overlay]() -> Element {
        const Theme& t = GetTheme();

        // ── History sidebar
        // ────────────────────────────────────────────────────
        std::vector<Element> hist_items;
        for (const auto& h : state->history) {
          std::string display = h.size() > 22 ? h.substr(0, 19) + "..." : h;
          hist_items.push_back(text(display) | color(t.text_muted));
        }
        if (hist_items.empty()) {
          hist_items.push_back(text("(no history)") | color(t.text_muted) |
                               dim);
        }
        auto sidebar =
            window(text(" History ") | bold | color(t.primary),
                   vbox(std::move(hist_items)) | vscroll_indicator | yflex) |
            size(WIDTH, EQUAL, 26) | yflex_grow;

        // ── Generated preview
        // ──────────────────────────────────────────────────
        Element preview;
        if (state->current_component) {
          preview = state->current_component->Render();
        } else {
          preview = text("Type a prompt to generate a component") | dim |
                    hcenter | vcenter;
        }
        auto main_area = window(text(" Preview: " + state->last_prompt + " ") |
                                    bold | color(t.primary),
                                preview | flex) |
                         flex;

        // ── Input row
        // ──────────────────────────────────────────────────────────
        auto input_row = hbox({
            text(" ❯ ") | bold | color(t.accent),
            text_input->Render() | flex,
        });

        // ── Status bar
        // ─────────────────────────────────────────────────────────
        auto status_bar =
            hbox({
                text(" NLP: ") | color(t.text_muted),
                text(state->backend_label) | bold | color(t.secondary),
                filler(),
                text(" Ctrl+H: help ") | color(t.text_muted),
            }) |
            color(t.text_muted);

        // ── Layout
        // ─────────────────────────────────────────────────────────────
        Element body = vbox({
            hbox({sidebar, main_area}) | flex,
            separator(),
            input_row,
            separator(),
            status_bar,
        });

        if (state->show_help) {
          body = dbox({body, help_overlay->Render() | clear_under | center});
        }
        return body;
      });

  // Ctrl+H toggles help overlay
  auto with_events = CatchEvent(root, [state](Event ev) -> bool {
    if (ev == Event::CtrlH) {
      state->show_help = !state->show_help;
      return true;
    }
    return false;
  });

  return with_events;
}

// ── WithLLMHelp
// ───────────────────────────────────────────────────────────────

Component WithLLMHelp(Component inner, const std::string& component_name) {
  auto show_help = std::make_shared<bool>(false);

  auto help_btn = Button(
      "?", [show_help] { *show_help = !*show_help; }, ButtonOption::Animated());

  auto help_overlay = Renderer([component_name, show_help]() -> Element {
    if (!*show_help) {
      return emptyElement();
    }
    const Theme& t = GetTheme();
    UIIntent info_intent;
    info_intent.raw_input = component_name;
    info_intent.type = IntentType::UNKNOWN;

    return window(
               text(" About: " + component_name + " ") | bold |
                   color(t.primary),
               vbox({
                   text("Component: " + component_name) | bold,
                   separatorEmpty(),
                   text(
                       "Use natural language to generate similar components:") |
                       color(t.text_muted),
                   separatorEmpty(),
                   text("  e.g. \"show me a " + component_name + "\"") |
                       color(t.secondary),
                   separatorEmpty(),
                   text("Press '?' to close") | color(t.text_muted),
               })) |
           clear_under | center;
  });

  auto buttons = Container::Horizontal({help_btn});

  return Renderer(buttons,
                  [inner, buttons, help_overlay, show_help]() -> Element {
                    auto inner_elem = inner->Render();
                    auto btn_elem = hbox({filler(), buttons->Render()});
                    Element body = vbox({btn_elem, inner_elem | flex});
                    if (*show_help) {
                      body = dbox({body, help_overlay->Render()});
                    }
                    return body;
                  });
}

}  // namespace ftxui::ui
