// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/voice_theme.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/nl_theme.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/ui/voice.hpp"

namespace ftxui::ui {

namespace {

std::string ToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return out;
}

std::string Trim(const std::string& s) {
  const auto begin = s.find_first_not_of(" \t\r\n");
  if (begin == std::string::npos) {
    return "";
  }
  const auto end = s.find_last_not_of(" \t\r\n");
  return s.substr(begin, end - begin + 1);
}

// Strip leading articles: "the", "a", "an"
std::string StripArticles(const std::string& s) {
  std::string out = s;
  for (const auto& article : {"the ", "a ", "an "}) {
    if (out.substr(0, std::string(article).size()) == article) {
      out = out.substr(std::string(article).size());
    }
  }
  return out;
}

// Strip trailing " theme"
std::string StripTrailingTheme(const std::string& s) {
  const std::string suffix = " theme";
  if (s.size() >= suffix.size() &&
      s.substr(s.size() - suffix.size()) == suffix) {
    return s.substr(0, s.size() - suffix.size());
  }
  return s;
}

}  // namespace

// ── VoiceThemeCommand ParseCommand
// ────────────────────────────────────────────

VoiceThemeCommand VoiceThemeController::ParseCommand(const std::string& text) {
  VoiceThemeCommand cmd;
  cmd.raw_text = text;

  const std::string lower = ToLower(text);

  // Multi-word wake words first (longer matches take priority)
  const std::vector<std::string> multi_word = {"switch to", "change to"};
  for (const auto& ww : multi_word) {
    const auto pos = lower.find(ww);
    if (pos != std::string::npos) {
      std::string after = lower.substr(pos + ww.size());
      after = Trim(after);
      after = StripArticles(after);
      after = StripTrailingTheme(after);
      after = Trim(after);
      if (!after.empty()) {
        cmd.is_theme_command = true;
        cmd.theme_name = after;
        cmd.confidence = 0.9f;
        return cmd;
      }
    }
  }

  // Single-word wake words
  const std::vector<std::string> single_word = {"theme", "apply", "activate",
                                                "use", "set"};
  for (const auto& ww : single_word) {
    const auto pos = lower.find(ww);
    if (pos != std::string::npos) {
      std::string after = lower.substr(pos + ww.size());
      after = Trim(after);
      after = StripArticles(after);
      after = StripTrailingTheme(after);
      after = Trim(after);
      if (!after.empty()) {
        cmd.is_theme_command = true;
        cmd.theme_name = after;
        cmd.confidence = 0.8f;
        return cmd;
      }
    }
  }

  return cmd;
}

// ── VoiceThemeController NamedTheme
// ───────────────────────────────────────────

Theme VoiceThemeController::NamedTheme(const std::string& name) {
  const std::string lower = ToLower(Trim(name));

  if (lower == "default" || lower == "badwolf") {
    return Theme::Default();
  }
  if (lower == "dark") {
    return Theme::Dark();
  }
  if (lower == "light") {
    return Theme::Light();
  }
  if (lower == "nord") {
    return Theme::Nord();
  }
  if (lower == "dracula") {
    return Theme::Dracula();
  }
  if (lower == "monokai") {
    return Theme::Monokai();
  }
  if (lower == "lcars") {
    return Theme::LCARS();
  }
  if (lower == "imperial" || lower == "empire" || lower == "sith") {
    return Theme::Imperial();
  }
  if (lower == "rebel" || lower == "rebellion") {
    return Theme::Rebel();
  }
  if (lower == "enterprise" || lower == "starfleet" || lower == "trek") {
    return Theme::Enterprise();
  }
  if (lower == "matrix" || lower == "hacker" || lower == "neo") {
    return Theme::Matrix();
  }

  // Fall back to NL description parser
  return NLThemeParser::FromDescription(name);
}

// ── VoiceThemeController AvailableThemeNames
// ──────────────────────────────────

std::vector<std::string> VoiceThemeController::AvailableThemeNames() {
  return {"default", "dark",     "light", "nord",       "dracula", "monokai",
          "lcars",   "imperial", "rebel", "enterprise", "matrix"};
}

// ── VoiceThemeController constructor/destructor
// ───────────────────────────────

VoiceThemeController::VoiceThemeController(VoiceThemeConfig cfg)
    : cfg_(std::move(cfg)),
      current_theme_(GetTheme()),
      target_theme_(current_theme_) {
  engine_ = std::make_unique<VoiceCommandEngine>(cfg_.voice);
  engine_->OnTranscription(
      [this](const std::string& text) { OnTranscription(text); });
}

VoiceThemeController::~VoiceThemeController() {
  if (frame_cb_id_ >= 0) {
    AnimationLoop::Instance().RemoveCallback(frame_cb_id_);
    frame_cb_id_ = -1;
  }
  Stop();
}

// ── Start / Stop / IsListening
// ────────────────────────────────────────────────

void VoiceThemeController::Start() {
  engine_->StartListening();
}

void VoiceThemeController::Stop() {
  engine_->StopListening();
}

bool VoiceThemeController::IsListening() const {
  return engine_->IsListening();
}

// ── ApplyTheme variants
// ───────────────────────────────────────────────────────

void VoiceThemeController::ApplyTheme(const Theme& theme) {
  StartTransition(theme, "");
}

void VoiceThemeController::ApplyThemeByName(const std::string& name) {
  StartTransition(NamedTheme(name), name);
}

void VoiceThemeController::ApplyThemeFromDescription(const std::string& desc) {
  StartTransition(NLThemeParser::FromDescription(desc), desc);
}

// ── Accessors
// ─────────────────────────────────────────────────────────────────

VoiceStatus VoiceThemeController::voice_status() const {
  return engine_->status();
}

const Theme& VoiceThemeController::current_theme() const {
  return current_theme_;
}

std::string VoiceThemeController::last_command() const {
  return last_command_;
}

float VoiceThemeController::transition_progress() const {
  if (!transition_) {
    return 1.0f;
  }
  return transition_->Progress();
}

// ── Callback registration
// ─────────────────────────────────────────────────────

int VoiceThemeController::OnThemeChanged(
    std::function<void(const Theme&, const std::string&)> fn) {
  const int id = next_cb_id_++;
  theme_callbacks_[id] = std::move(fn);
  return id;
}

int VoiceThemeController::OnCommand(
    std::function<void(const VoiceThemeCommand&)> fn) {
  const int id = next_cb_id_++;
  cmd_callbacks_[id] = std::move(fn);
  return id;
}

void VoiceThemeController::RemoveCallback(int id) {
  theme_callbacks_.erase(id);
  cmd_callbacks_.erase(id);
}

// ── Internal: OnTranscription
// ─────────────────────────────────────────────────

void VoiceThemeController::OnTranscription(const std::string& text) {
  VoiceThemeCommand cmd = ParseCommand(text);
  cmd.raw_text = text;

  for (auto& [id, fn] : cmd_callbacks_) {
    fn(cmd);
  }

  if (cmd.is_theme_command && !cmd.theme_name.empty()) {
    Theme new_theme = NamedTheme(cmd.theme_name);
    last_command_ = text;
    StartTransition(new_theme, cmd.theme_name);
  }
}

// ── Internal: StartTransition
// ─────────────────────────────────────────────────

void VoiceThemeController::StartTransition(const Theme& target,
                                           const std::string& desc) {
  // Remove any existing frame callback
  if (frame_cb_id_ >= 0) {
    AnimationLoop::Instance().RemoveCallback(frame_cb_id_);
    frame_cb_id_ = -1;
  }

  target_theme_ = target;
  transition_ = std::make_unique<ThemeTransition>(current_theme_, target,
                                                  cfg_.transition_duration);
  transition_->Start();

  const std::string captured_desc = desc;

  frame_cb_id_ =
      AnimationLoop::Instance().OnFrame([this, captured_desc](float /*dt*/) {
        if (!transition_) {
          return;
        }

        SetTheme(transition_->Current());

        if (transition_->Complete()) {
          current_theme_ = target_theme_;
          SetTheme(target_theme_);

          // Fire callbacks
          for (auto& [id, fn] : theme_callbacks_) {
            fn(current_theme_, captured_desc);
          }

          // Remove frame callback
          if (frame_cb_id_ >= 0) {
            AnimationLoop::Instance().RemoveCallback(frame_cb_id_);
            frame_cb_id_ = -1;
          }
          transition_.reset();

          // Trigger repaint
          if (auto* app = App::Active()) {
            app->PostEvent(Event::Custom);
          }
        } else {
          if (auto* app = App::Active()) {
            app->PostEvent(Event::Custom);
          }
        }
      });
}

// ── LiveThemeBar
// ──────────────────────────────────────────────────────────────

ftxui::Component LiveThemeBar(std::shared_ptr<VoiceThemeController> ctrl,
                              VoiceThemeConfig cfg) {
  if (!ctrl) {
    ctrl = std::make_shared<VoiceThemeController>(std::move(cfg));
  }

  struct State {
    std::shared_ptr<VoiceThemeController> ctrl;
    std::string input_text;
  };

  auto state = std::make_shared<State>();
  state->ctrl = ctrl;

  auto input_component = Input(&state->input_text, "theme name…");

  auto bar = Container::Horizontal({
      input_component,
  });

  auto renderer = Renderer(bar, [state, input_component]() -> Element {
    const Theme& t = GetTheme();
    const VoiceStatus vs = state->ctrl->voice_status();
    const bool listening = state->ctrl->IsListening();

    // Mic button
    std::string mic_label = listening ? "🔴" : "🎙";
    Element mic_elem = text(" " + mic_label + " V ") | bold;
    if (listening) {
      mic_elem = mic_elem | color(Color::Red);
    } else {
      mic_elem = mic_elem | color(t.text_muted);
    }

    // Theme name indicator
    // We show the current theme primary color name as placeholder
    std::string theme_label = state->ctrl->last_command();
    if (theme_label.empty()) {
      theme_label = "default";
    }
    Element theme_elem = text(" ● " + theme_label + " ") | color(t.primary);

    // Color swatches
    Elements swatches;
    swatches.push_back(text("█") | color(t.primary));
    swatches.push_back(text("█") | color(t.secondary));
    swatches.push_back(text("█") | color(t.accent));
    swatches.push_back(text("█") | color(t.error_color));
    swatches.push_back(text("█") | color(t.warning_color));
    Element swatch_elem = hbox(std::move(swatches));

    // Last command text
    const std::string& last_cmd = state->ctrl->last_command();
    Element cmd_elem = text(last_cmd.empty() ? "" : " \"" + last_cmd + "\"") |
                       color(t.text_muted);

    // Input field
    Element input_elem = hbox({
        text(" [") | color(t.border_color),
        input_component->Render(),
        text("] ") | color(t.border_color),
    });

    // Voice status text
    std::string status_str;
    switch (vs) {
      case VoiceStatus::Idle:
        status_str = "[whisper: idle]";
        break;
      case VoiceStatus::Listening:
        status_str = "[whisper: 🎙]";
        break;
      case VoiceStatus::Processing:
        status_str = "[whisper: ⟳]";
        break;
      case VoiceStatus::Ready:
        status_str = "[whisper: ready]";
        break;
      case VoiceStatus::Error:
        status_str = "[whisper: err]";
        break;
      case VoiceStatus::Unavailable:
        status_str = "[whisper: n/a]";
        break;
    }
    Element status_elem = text(" " + status_str) | color(t.text_muted);

    return hbox({
               mic_elem,
               theme_elem,
               swatch_elem,
               cmd_elem,
               filler(),
               input_elem,
               status_elem,
           }) |
           border;
  });

  // Catch events for V key and Enter in input
  auto wrapped = CatchEvent(renderer, [state, input_component](Event event) {
    if (event == Event::Character('V') || event == Event::Character('v')) {
      if (state->ctrl->IsListening()) {
        state->ctrl->Stop();
      } else {
        state->ctrl->Start();
      }
      return true;
    }
    if (event == Event::Return && !state->input_text.empty()) {
      state->ctrl->ApplyThemeByName(state->input_text);
      state->input_text.clear();
      return true;
    }
    return false;
  });

  return wrapped;
}

// ── ThemeStage
// ────────────────────────────────────────────────────────────────

ftxui::Component ThemeStage() {
  struct State {
    std::string input_text;
  };
  auto state = std::make_shared<State>();
  auto input = Input(&state->input_text, "type here…");

  auto container = Container::Vertical({input});

  return Renderer(container, [state, input]() -> Element {
    const Theme& t = GetTheme();

    // Header
    Element header =
        text("  Theme Preview  ") | bold | color(t.primary) | border;

    // Buttons row
    auto btn_style_normal = ButtonOption::Simple();
    auto btn_style_focused = ButtonOption::Simple();
    btn_style_normal.transform = [&t](const EntryState& s) -> Element {
      auto e = text(s.label);
      if (s.focused) {
        e = e | color(t.button_fg_active) | bgcolor(t.button_bg_active);
      } else {
        e = e | color(t.button_fg_normal);
      }
      return e | border;
    };

    Element buttons_row = hbox({
        text("  [Button]  ") | color(t.button_fg_normal) | border,
        text("  [Active]  ") | color(t.button_fg_active) |
            bgcolor(t.button_bg_active) | border,
        text("  [Error]   ") | color(t.error_color) | border,
        text("  [Success] ") | color(t.success_color) | border,
    });

    // Input area
    Element input_area = hbox({
                             text("Input: ") | color(t.text_muted),
                             input->Render() | flex,
                         }) |
                         border;

    // Data table mock
    Element table_header = hbox({
        text(" Name       ") | bold | color(t.primary),
        text(" │ ") | color(t.border_color),
        text(" Value      ") | bold | color(t.primary),
        text(" │ ") | color(t.border_color),
        text(" Status     ") | bold | color(t.primary),
    });
    Element table_row1 = hbox({
        text(" Alpha      ") | color(t.text),
        text(" │ ") | color(t.border_color),
        text(" 42         ") | color(t.accent),
        text(" │ ") | color(t.border_color),
        text(" OK         ") | color(t.success_color),
    });
    Element table_row2 = hbox({
        text(" Beta       ") | color(t.text),
        text(" │ ") | color(t.border_color),
        text(" 7          ") | color(t.accent),
        text(" │ ") | color(t.border_color),
        text(" WARN       ") | color(t.warning_color),
    });
    Element table_row3 = hbox({
        text(" Gamma      ") | color(t.text),
        text(" │ ") | color(t.border_color),
        text(" 0          ") | color(t.accent),
        text(" │ ") | color(t.border_color),
        text(" ERR        ") | color(t.error_color),
    });
    Element data_table =
        vbox({table_header, separator(), table_row1, table_row2, table_row3}) |
        border;

    // Status bar
    Element status_bar = hbox({
                             text(" ● primary ") | color(t.primary),
                             text(" ● secondary ") | color(t.secondary),
                             text(" ● accent ") | color(t.accent),
                             text(" ● error ") | color(t.error_color),
                             text(" ● warning ") | color(t.warning_color),
                             text(" ● success ") | color(t.success_color),
                             filler(),
                         }) |
                         color(t.text_muted);

    return vbox({
               header | hcenter,
               buttons_row,
               input_area,
               data_table,
               status_bar,
           }) |
           flex;
  });
}

// ── WithLiveThemes
// ────────────────────────────────────────────────────────────

ftxui::Component WithLiveThemes(ftxui::Component app,
                                std::shared_ptr<VoiceThemeController> ctrl,
                                VoiceThemeConfig cfg) {
  if (!ctrl) {
    ctrl = std::make_shared<VoiceThemeController>(cfg);
  }

  auto bar = LiveThemeBar(ctrl, cfg);
  auto layout = Container::Vertical({
      app,
      bar,
  });

  return CatchEvent(layout, [ctrl](Event event) {
    if (event == Event::Character('V') || event == Event::Character('v')) {
      if (ctrl->IsListening()) {
        ctrl->Stop();
      } else {
        ctrl->Start();
      }
      return true;
    }
    return false;
  });
}

// ── ThemeShowcase
// ─────────────────────────────────────────────────────────────

ftxui::Component ThemeShowcase(bool auto_cycle,
                               std::chrono::milliseconds cycle_interval) {
  struct State {
    int theme_index = 0;
    float accum = 0.0f;
    int frame_cb_id = -1;
    std::shared_ptr<VoiceThemeController> ctrl;
  };

  auto state = std::make_shared<State>();
  state->ctrl = std::make_shared<VoiceThemeController>();

  const std::vector<std::string> names =
      VoiceThemeController::AvailableThemeNames();

  // Build theme buttons
  std::vector<Component> buttons;
  for (size_t i = 0; i < names.size(); ++i) {
    auto name = names[i];
    auto s = state;
    auto btn = Button(name, [s, name]() { s->ctrl->ApplyThemeByName(name); });
    buttons.push_back(btn);
  }

  auto stage = ThemeStage();

  auto btn_row = Container::Horizontal(buttons);
  auto layout = Container::Vertical({btn_row, stage});

  if (auto_cycle) {
    const float interval_seconds =
        static_cast<float>(cycle_interval.count()) / 1000.0f;

    state->frame_cb_id = AnimationLoop::Instance().OnFrame(
        [state, names, interval_seconds](float dt) {
          state->accum += dt;
          if (state->accum >= interval_seconds) {
            state->accum = 0.0f;
            state->theme_index =
                (state->theme_index + 1) % static_cast<int>(names.size());
            state->ctrl->ApplyThemeByName(names[state->theme_index]);
            if (auto* app = App::Active()) {
              app->PostEvent(Event::Custom);
            }
          }
        });
  }

  return layout;
}

// ── ReactiveTheme
// ─────────────────────────────────────────────────────────────

ReactiveTheme& ReactiveTheme::Global() {
  static ReactiveTheme instance;
  return instance;
}

const Theme& ReactiveTheme::Get() const {
  return current_;
}

void ReactiveTheme::Set(const Theme& t, bool animate) {
  if (animate) {
    // Remove existing frame callback
    if (frame_cb_id_ >= 0) {
      AnimationLoop::Instance().RemoveCallback(frame_cb_id_);
      frame_cb_id_ = -1;
    }

    Theme from = current_;
    transition_ = std::make_unique<ThemeTransition>(
        from, t, std::chrono::milliseconds(500));
    transition_->Start();

    frame_cb_id_ = AnimationLoop::Instance().OnFrame([this, t](float /*dt*/) {
      if (!transition_) {
        return;
      }

      SetTheme(transition_->Current());

      if (transition_->Complete()) {
        current_ = t;
        SetTheme(t);
        for (auto& [id, fn] : listeners_) {
          fn(current_);
        }
        if (frame_cb_id_ >= 0) {
          AnimationLoop::Instance().RemoveCallback(frame_cb_id_);
          frame_cb_id_ = -1;
        }
        transition_.reset();
        if (auto* app = App::Active()) {
          app->PostEvent(Event::Custom);
        }
      } else {
        if (auto* app = App::Active()) {
          app->PostEvent(Event::Custom);
        }
      }
    });
  } else {
    current_ = t;
    SetTheme(t);
    for (auto& [id, fn] : listeners_) {
      fn(current_);
    }
  }
}

int ReactiveTheme::OnChange(std::function<void(const Theme&)> fn) {
  const int id = next_id_++;
  listeners_[id] = std::move(fn);
  return id;
}

void ReactiveTheme::RemoveOnChange(int id) {
  listeners_.erase(id);
}

ftxui::Component ReactiveTheme::Bind(ftxui::Component c) {
  return Renderer(c, [c]() -> Element {
    // Reading GetTheme() each frame ensures live updates
    (void)GetTheme();
    return c->Render();
  });
}

}  // namespace ftxui::ui
