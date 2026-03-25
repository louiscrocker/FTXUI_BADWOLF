// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file typewriter.cpp
/// Implementation of TypewriterText, TypewriterSequence, Console, and
/// ConsolePrompt for FTXUI BadWolf high-level UI.

#include "ftxui/ui/typewriter.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/animation.hpp"
#include "ftxui/ui/theme.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── TypewriterText ─────────────────────────────────────────────────────────

Component TypewriterText(std::string text, TypewriterConfig cfg) {
  struct State {
    std::string text;
    TypewriterConfig cfg;
    int displayed_count = 0;
    bool complete_fired = false;
    float elapsed_s = 0.0f;
    float chars_accumulated = 0.0f;
    std::mutex mutex;
    int callback_id = -1;

    ~State() {
      if (callback_id >= 0) {
        AnimationLoop::Instance().RemoveCallback(callback_id);
      }
    }
  };

  auto state = std::make_shared<State>();
  state->text = std::move(text);
  state->cfg = cfg;

  // Instant display for non-animated mode.
  if (cfg.chars_per_second <= 0) {
    state->displayed_count = static_cast<int>(state->text.size());
    state->complete_fired = true;
    if (cfg.on_complete) {
      cfg.on_complete();
    }
  }

  if (cfg.chars_per_second > 0) {
    auto weak = std::weak_ptr<State>(state);
    state->callback_id = AnimationLoop::Instance().OnFrame([weak](float dt) {
      auto s = weak.lock();
      if (!s) {
        return;
      }
      std::lock_guard<std::mutex> lock(s->mutex);
      s->elapsed_s += dt;
      if (s->displayed_count < static_cast<int>(s->text.size())) {
        s->chars_accumulated +=
            static_cast<float>(s->cfg.chars_per_second) * dt;
        int to_add = static_cast<int>(s->chars_accumulated);
        s->chars_accumulated -= static_cast<float>(to_add);
        s->displayed_count = std::min(s->displayed_count + to_add,
                                      static_cast<int>(s->text.size()));
        if (s->displayed_count >= static_cast<int>(s->text.size()) &&
            !s->complete_fired) {
          s->complete_fired = true;
          if (s->cfg.on_complete) {
            s->cfg.on_complete();
          }
        }
      }
    });
  }

  return Renderer([state]() -> Element {
    std::lock_guard<std::mutex> lock(state->mutex);

    std::string displayed = state->text.substr(0, state->displayed_count);

    // Determine cursor visibility.
    bool cursor_visible = false;
    if (state->cfg.show_cursor) {
      bool still_typing =
          state->displayed_count < static_cast<int>(state->text.size());
      if (still_typing) {
        if (state->cfg.cursor_blink && state->cfg.blink_hz > 0) {
          int phase = static_cast<int>(state->elapsed_s *
                                       static_cast<float>(state->cfg.blink_hz));
          cursor_visible = (phase % 2 == 0);
        } else {
          cursor_visible = true;
        }
      }
    }

    if (cursor_visible) {
      displayed += state->cfg.cursor_char;
    }

    return ftxui::text(displayed);
  });
}

// ── TypewriterSequence ────────────────────────────────────────────────────

TypewriterSequence& TypewriterSequence::Then(std::string txt,
                                             TypewriterConfig cfg) {
  Step s;
  s.text = std::move(txt);
  s.cfg = cfg;
  s.is_pause = false;
  s.is_clear = false;
  steps_.push_back(std::move(s));
  return *this;
}

TypewriterSequence& TypewriterSequence::Pause(
    std::chrono::milliseconds duration) {
  Step s;
  s.is_pause = true;
  s.pause_dur = duration;
  steps_.push_back(std::move(s));
  return *this;
}

TypewriterSequence& TypewriterSequence::Clear() {
  Step s;
  s.is_clear = true;
  steps_.push_back(std::move(s));
  return *this;
}

TypewriterSequence& TypewriterSequence::OnComplete(std::function<void()> fn) {
  on_complete_ = std::move(fn);
  return *this;
}

Component TypewriterSequence::Build() {
  struct SeqState {
    std::vector<Step> steps;
    std::function<void()> on_complete;
    int current_step = 0;
    std::string displayed_text;  // accumulated from completed text steps
    int current_chars = 0;       // chars displayed in current step
    float chars_acc = 0.0f;
    float pause_elapsed = 0.0f;
    float elapsed_s = 0.0f;
    bool complete_fired = false;
    std::mutex mutex;
    int callback_id = -1;

    ~SeqState() {
      if (callback_id >= 0) {
        AnimationLoop::Instance().RemoveCallback(callback_id);
      }
    }
  };

  auto state = std::make_shared<SeqState>();
  state->steps = steps_;
  state->on_complete = on_complete_;

  if (!steps_.empty()) {
    auto weak = std::weak_ptr<SeqState>(state);
    state->callback_id = AnimationLoop::Instance().OnFrame([weak](float dt) {
      auto s = weak.lock();
      if (!s) {
        return;
      }
      std::lock_guard<std::mutex> lock(s->mutex);

      if (s->complete_fired) {
        return;
      }
      if (s->current_step >= static_cast<int>(s->steps.size())) {
        if (!s->complete_fired) {
          s->complete_fired = true;
          if (s->on_complete) {
            s->on_complete();
          }
        }
        return;
      }

      s->elapsed_s += dt;
      const Step& step = s->steps[s->current_step];

      if (step.is_clear) {
        s->displayed_text.clear();
        s->current_step++;
        return;
      }

      if (step.is_pause) {
        s->pause_elapsed += dt;
        float dur_s = static_cast<float>(step.pause_dur.count()) / 1000.0f;
        if (s->pause_elapsed >= dur_s) {
          s->pause_elapsed = 0.0f;
          s->current_step++;
        }
        return;
      }

      // Text typing step.
      int cps =
          step.cfg.chars_per_second > 0 ? step.cfg.chars_per_second : 9999;
      s->chars_acc += static_cast<float>(cps) * dt;
      int to_add = static_cast<int>(s->chars_acc);
      s->chars_acc -= static_cast<float>(to_add);
      int max_chars = static_cast<int>(step.text.size());

      // Fire on_char callbacks.
      int old_count = s->current_chars;
      s->current_chars = std::min(s->current_chars + to_add, max_chars);
      for (int i = old_count; i < s->current_chars; ++i) {
        if (step.cfg.on_char) {
          step.cfg.on_char(step.text[i]);
        }
      }

      if (s->current_chars >= max_chars) {
        // Step complete.
        s->displayed_text += step.text;
        if (step.cfg.on_complete) {
          step.cfg.on_complete();
        }
        s->current_step++;
        s->current_chars = 0;
        s->chars_acc = 0.0f;
      }
    });
  }

  return Renderer([state]() -> Element {
    std::lock_guard<std::mutex> lock(state->mutex);

    std::string render_text = state->displayed_text;

    // Add partial current step text.
    if (state->current_step < static_cast<int>(state->steps.size())) {
      const Step& step = state->steps[state->current_step];
      if (!step.is_pause && !step.is_clear) {
        render_text += step.text.substr(0, state->current_chars);

        // Cursor for current step.
        if (step.cfg.show_cursor) {
          bool still_typing =
              state->current_chars < static_cast<int>(step.text.size());
          if (still_typing) {
            bool cursor_vis = true;
            if (step.cfg.cursor_blink && step.cfg.blink_hz > 0) {
              int phase = static_cast<int>(
                  state->elapsed_s * static_cast<float>(step.cfg.blink_hz));
              cursor_vis = (phase % 2 == 0);
            }
            if (cursor_vis) {
              render_text += step.cfg.cursor_char;
            }
          }
        }
      }
    }

    return text(render_text);
  });
}

// ── Console ───────────────────────────────────────────────────────────────

struct Console::Impl {
  // Using shared_ptr<ConsoleLine> so TypePrint can hold a reference while
  // adding characters from a background thread.
  std::deque<std::shared_ptr<ConsoleLine>> lines;
  mutable std::mutex mutex;
  int max_lines;
};

std::shared_ptr<Console> Console::Create(int max_lines) {
  auto c = std::shared_ptr<Console>(new Console());
  c->impl_ = std::make_shared<Impl>();
  c->impl_->max_lines = max_lines;
  return c;
}

void Console::Print(const std::string& text_str,
                    ftxui::Color fg,
                    ftxui::Color bg) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->lines.empty()) {
    auto line = std::make_shared<ConsoleLine>();
    line->fg = fg;
    line->bg = bg;
    impl_->lines.push_back(line);
  }
  impl_->lines.back()->text += text_str;
  impl_->lines.back()->fg = fg;
  impl_->lines.back()->bg = bg;
}

void Console::PrintLine(const std::string& text_str,
                        ftxui::Color fg,
                        ftxui::Color bg) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  auto line = std::make_shared<ConsoleLine>();
  line->text = text_str;
  line->fg = fg;
  line->bg = bg;
  impl_->lines.push_back(std::move(line));
  while (static_cast<int>(impl_->lines.size()) > impl_->max_lines) {
    impl_->lines.pop_front();
  }
}

void Console::PrintBold(const std::string& text_str, ftxui::Color fg) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  auto line = std::make_shared<ConsoleLine>();
  line->text = text_str;
  line->fg = fg;
  line->bold = true;
  impl_->lines.push_back(std::move(line));
  while (static_cast<int>(impl_->lines.size()) > impl_->max_lines) {
    impl_->lines.pop_front();
  }
}

void Console::PrintDim(const std::string& text_str, ftxui::Color fg) {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  auto line = std::make_shared<ConsoleLine>();
  line->text = text_str;
  line->fg = fg;
  line->dim = true;
  impl_->lines.push_back(std::move(line));
  while (static_cast<int>(impl_->lines.size()) > impl_->max_lines) {
    impl_->lines.pop_front();
  }
}

void Console::Info(const std::string& msg) {
  PrintLine("[INFO] " + msg, ftxui::Color::Cyan);
}

void Console::Warn(const std::string& msg) {
  PrintLine("[WARN] " + msg, ftxui::Color::Yellow);
}

void Console::Error(const std::string& msg) {
  auto impl = impl_;
  std::lock_guard<std::mutex> lock(impl->mutex);
  auto line = std::make_shared<ConsoleLine>();
  line->text = "[ERROR] " + msg;
  line->fg = ftxui::Color::Red;
  line->bold = true;
  impl->lines.push_back(std::move(line));
  while (static_cast<int>(impl->lines.size()) > impl->max_lines) {
    impl->lines.pop_front();
  }
}

void Console::Success(const std::string& msg) {
  PrintLine("[OK] " + msg, ftxui::Color::Green);
}

void Console::TypePrint(const std::string& text_str,
                        TypewriterConfig cfg,
                        ftxui::Color fg) {
  if (text_str.empty()) {
    return;
  }
  auto impl = impl_;

  // Create a shared line that the background thread will write into.
  auto line = std::make_shared<ConsoleLine>();
  line->fg = fg;

  {
    std::lock_guard<std::mutex> lock(impl->mutex);
    impl->lines.push_back(line);
    while (static_cast<int>(impl->lines.size()) > impl->max_lines) {
      impl->lines.pop_front();
    }
  }

  std::thread([impl, line, text_str, cfg]() {
    int delay_us =
        cfg.chars_per_second > 0 ? 1'000'000 / cfg.chars_per_second : 0;
    for (char c : text_str) {
      {
        std::lock_guard<std::mutex> lock(impl->mutex);
        line->text += c;
      }
      if (cfg.on_char) {
        cfg.on_char(c);
      }
      if (delay_us > 0) {
        std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
      }
      // Request UI refresh.
      if (App* a = App::Active()) {
        a->Post([] {});
      }
    }
    if (cfg.on_complete) {
      cfg.on_complete();
    }
  }).detach();
}

void Console::Clear() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  impl_->lines.clear();
}

int Console::LineCount() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return static_cast<int>(impl_->lines.size());
}

std::vector<ConsoleLine> Console::Lines() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  std::vector<ConsoleLine> result;
  result.reserve(impl_->lines.size());
  for (const auto& ptr : impl_->lines) {
    result.push_back(*ptr);
  }
  return result;
}

Component Console::Build(std::string_view title) const {
  auto impl = impl_;
  std::string title_str{title};

  return Renderer([impl, title_str]() -> Element {
    std::vector<std::shared_ptr<ConsoleLine>> snapshot;
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      snapshot.assign(impl->lines.begin(), impl->lines.end());
    }

    Elements elems;
    elems.reserve(snapshot.size());
    for (const auto& ln : snapshot) {
      Element e = ftxui::text(ln->text);
      if (ln->fg != ftxui::Color::Default) {
        e = e | ftxui::color(ln->fg);
      }
      if (ln->bg != ftxui::Color::Default) {
        e = e | ftxui::bgcolor(ln->bg);
      }
      if (ln->bold) {
        e = e | ftxui::bold;
      }
      if (ln->dim) {
        e = e | ftxui::dim;
      }
      if (ln->italic) {
        e = e | ftxui::italic;
      }
      elems.push_back(e);
    }

    if (!elems.empty()) {
      elems.back() = elems.back() | focus;
    }

    Element body = vbox(std::move(elems)) | vscroll_indicator | yframe | flex;

    if (title_str.empty()) {
      return body;
    }

    const Theme& t = GetTheme();
    return window(ftxui::text(" " + title_str + " "), std::move(body),
                  t.border_style);
  });
}

// ── ConsolePrompt ─────────────────────────────────────────────────────────

Component ConsolePrompt(std::shared_ptr<Console> console,
                        ConsolePromptConfig cfg) {
  auto input_content = std::make_shared<std::string>();
  auto history = std::make_shared<std::vector<std::string>>();
  auto history_idx = std::make_shared<int>(-1);

  InputOption opt;
  opt.placeholder = cfg.placeholder;
  auto input = Input(input_content.get(), opt);

  // Capture cfg by value for use in lambda.
  auto console_ptr = console;
  auto cfg_copy = cfg;

  input |= CatchEvent([=](Event e) mutable {
    if (e == Event::Return) {
      std::string cmd = *input_content;
      if (cmd.empty()) {
        return true;
      }

      // Add to history.
      history->push_back(cmd);
      if (static_cast<int>(history->size()) > cfg_copy.history_size) {
        history->erase(history->begin());
      }
      *history_idx = -1;

      // Echo the command.
      console_ptr->PrintLine(cfg_copy.prompt_str + cmd, cfg_copy.prompt_color);

      // Get and display response.
      if (cfg_copy.handler) {
        std::string response = cfg_copy.handler(cmd);
        if (!response.empty()) {
          if (cfg_copy.typewriter_output) {
            console_ptr->TypePrint(response, cfg_copy.typewriter_cfg);
          } else {
            console_ptr->PrintLine(response);
          }
        }
      }

      *input_content = "";
      return true;
    }

    // Up arrow — navigate history backwards.
    if (e == Event::ArrowUp && !history->empty()) {
      if (*history_idx < static_cast<int>(history->size()) - 1) {
        (*history_idx)++;
        *input_content =
            (*history)[history->size() - 1 - static_cast<size_t>(*history_idx)];
      }
      return true;
    }

    // Down arrow — navigate history forwards.
    if (e == Event::ArrowDown) {
      if (*history_idx > 0) {
        (*history_idx)--;
        *input_content =
            (*history)[history->size() - 1 - static_cast<size_t>(*history_idx)];
      } else if (*history_idx == 0) {
        *history_idx = -1;
        *input_content = "";
      }
      return true;
    }

    return false;
  });

  auto console_comp = console->Build();
  auto container = Container::Vertical({console_comp, input});

  ftxui::Color prompt_color = cfg.prompt_color;
  std::string prompt_str = cfg.prompt_str;

  return Renderer(container, [=]() -> Element {
    Element prompt_elem = ftxui::text(prompt_str) | ftxui::color(prompt_color);
    Element input_line = hbox({prompt_elem, input->Render()});
    return vbox({
               console_comp->Render() | flex,
               ftxui::separatorLight(),
               input_line,
           }) |
           flex;
  });
}

// ── WithTypewriter ────────────────────────────────────────────────────────

Component WithTypewriter(Component inner,
                         std::string intro_text,
                         TypewriterConfig cfg) {
  auto showing = std::make_shared<std::atomic<bool>>(true);

  auto orig_complete = cfg.on_complete;
  cfg.on_complete = [showing, orig_complete]() {
    showing->store(false);
    if (App* a = App::Active()) {
      a->Post([] {});
    }
    if (orig_complete) {
      orig_complete();
    }
  };

  auto tw = TypewriterText(std::move(intro_text), cfg);

  return Renderer([tw, inner, showing]() -> Element {
    if (showing->load()) {
      return tw->Render();
    }
    return inner->Render();
  });
}

}  // namespace ftxui::ui
