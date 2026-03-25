// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/filepicker.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/theme.hpp"

namespace fs = std::filesystem;

namespace ftxui::ui {

// ── Internal state
// ────────────────────────────────────────────────────────────

struct FilePicker::Impl {
  fs::path current_dir;
  std::function<bool(const fs::path&)> filter;
  bool show_hidden = false;

  std::function<void(const fs::path&)> on_select;
  std::function<void(const fs::path&)> on_confirm;

  // Runtime
  std::vector<fs::path> entries;  // sorted list shown in the list box
  int selected = 0;
  fs::path selected_path;

  void Refresh() {
    entries.clear();
    selected = 0;

    std::error_code ec;
    if (!fs::is_directory(current_dir, ec)) {
      return;
    }

    std::vector<fs::path> dirs;
    std::vector<fs::path> files;

    for (const auto& entry : fs::directory_iterator(current_dir, ec)) {
      const fs::path& p = entry.path();
      std::string name = p.filename().string();

      // Hidden filter
      if (!show_hidden && !name.empty() && name[0] == '.') {
        continue;
      }

      if (fs::is_directory(p, ec)) {
        dirs.push_back(p);
      } else {
        // Apply file filter if set
        if (!filter || filter(p)) {
          files.push_back(p);
        }
      }
    }

    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    // ".." entry always first (unless we are at root)
    if (current_dir.has_parent_path() &&
        current_dir != current_dir.root_path()) {
      entries.push_back(current_dir / "..");
    }

    for (auto& d : dirs) {
      entries.push_back(std::move(d));
    }
    for (auto& f : files) {
      entries.push_back(std::move(f));
    }
  }
};

// ── Builder
// ───────────────────────────────────────────────────────────────────

FilePicker::FilePicker() : impl_(std::make_shared<Impl>()) {
  impl_->current_dir = fs::current_path();
}

FilePicker& FilePicker::StartDir(std::string_view path) {
  std::error_code ec;
  fs::path p(path);
  if (fs::is_directory(p, ec)) {
    impl_->current_dir = fs::canonical(p, ec);
    if (ec) {
      impl_->current_dir = fs::current_path();
    }
  } else {
    impl_->current_dir = fs::current_path();
  }
  return *this;
}

FilePicker& FilePicker::Filter(std::function<bool(const fs::path&)> fn) {
  impl_->filter = std::move(fn);
  return *this;
}

FilePicker& FilePicker::ShowHidden(bool v) {
  impl_->show_hidden = v;
  return *this;
}

FilePicker& FilePicker::OnSelect(std::function<void(const fs::path&)> fn) {
  impl_->on_select = std::move(fn);
  return *this;
}

FilePicker& FilePicker::OnConfirm(std::function<void(const fs::path&)> fn) {
  impl_->on_confirm = std::move(fn);
  return *this;
}

// ── Build
// ─────────────────────────────────────────────────────────────────────

ftxui::Component FilePicker::Build() {
  auto s = impl_;
  s->Refresh();

  // ── Renderer ──────────────────────────────────────────────────────────────
  auto renderer = ftxui::Renderer([s]() -> ftxui::Element {
    const Theme& theme = GetTheme();

    // Breadcrumb bar
    std::string crumb = s->current_dir.string();
    // Replace $HOME prefix with ~
    const char* home = std::getenv("HOME");
    if (home && crumb.rfind(home, 0) == 0) {
      crumb = "~" + crumb.substr(std::string(home).size());
    }

    ftxui::Element breadcrumb =
        ftxui::hbox(
            {ftxui::text(" "),
             ftxui::text(crumb) | ftxui::color(theme.primary) | ftxui::bold,
             ftxui::filler()}) |
        ftxui::bgcolor(ftxui::Color::Default);

    // File list
    ftxui::Elements rows;
    if (s->entries.empty()) {
      rows.push_back(ftxui::text(" (empty)") | ftxui::color(theme.text_muted));
    } else {
      for (int i = 0; i < static_cast<int>(s->entries.size()); ++i) {
        const fs::path& p = s->entries[static_cast<size_t>(i)];
        bool sel = (i == s->selected);

        std::error_code ec;
        bool is_dir = fs::is_directory(p, ec);

        // Determine display name
        std::string name;
        if (p.filename() == "..") {
          name = " ../";
        } else if (is_dir) {
          name = " " + p.filename().string() + "/";
        } else {
          name = " " + p.filename().string();
        }

        ftxui::Element row;
        if (is_dir || p.filename() == "..") {
          row = ftxui::text(name) | ftxui::color(theme.primary);
        } else {
          row = ftxui::text(name);
        }

        if (sel) {
          row = row | ftxui::bold | ftxui::bgcolor(ftxui::Color::GrayDark);
        }

        rows.push_back(std::move(row));
      }
    }

    ftxui::Element list_el = ftxui::vbox(std::move(rows)) |
                             ftxui::vscroll_indicator | ftxui::yframe |
                             ftxui::flex;

    // Selected path bar
    std::string sel_str = s->selected_path.empty()
                              ? " No file selected"
                              : " " + s->selected_path.string();
    ftxui::Element status =
        ftxui::hbox({ftxui::text(sel_str) | ftxui::color(theme.text_muted) |
                     ftxui::flex}) |
        ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1);

    return ftxui::vbox({
        breadcrumb,
        ftxui::separator(),
        list_el,
        ftxui::separator(),
        status,
    });
  });

  // ── Event handler ─────────────────────────────────────────────────────────
  return ftxui::CatchEvent(renderer, [s](ftxui::Event event) -> bool {
    const int n = static_cast<int>(s->entries.size());
    if (n == 0) {
      return false;
    }

    if (event == ftxui::Event::ArrowUp) {
      if (s->selected > 0) {
        s->selected--;
      }
      return true;
    }

    if (event == ftxui::Event::ArrowDown) {
      if (s->selected < n - 1) {
        s->selected++;
      }
      return true;
    }

    if (event == ftxui::Event::Return) {
      const fs::path& p = s->entries[static_cast<size_t>(s->selected)];
      std::error_code ec;
      if (fs::is_directory(p, ec)) {
        // Navigate into directory
        fs::path canonical = fs::canonical(p, ec);
        if (!ec) {
          s->current_dir = canonical;
        } else {
          s->current_dir = p;
        }
        s->Refresh();
      } else {
        // File selected
        s->selected_path = p;
        if (s->on_select) {
          s->on_select(p);
        }
        if (s->on_confirm) {
          s->on_confirm(p);
        }
      }
      return true;
    }

    // Backspace: go up a directory
    if (event == ftxui::Event::Backspace) {
      fs::path parent = s->current_dir.parent_path();
      if (parent != s->current_dir) {
        s->current_dir = parent;
        s->Refresh();
      }
      return true;
    }

    // 'h': toggle hidden files
    if (event.is_character() && event.character() == "h") {
      s->show_hidden = !s->show_hidden;
      s->Refresh();
      return true;
    }

    return false;
  });
}

}  // namespace ftxui::ui
