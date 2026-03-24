// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include "ftxui/ui/markdown.hpp"

#include <algorithm>
#include <cctype>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/theme.hpp"

namespace ftxui::ui {

namespace {

// ─── Inline span types ───────────────────────────────────────────────────────

enum class SpanKind { Plain, Bold, Italic, Code, Link };

struct Span {
  SpanKind kind;
  std::string text;
  std::string url;  // only for Link
};

// ─── Inline parser ───────────────────────────────────────────────────────────

// Parse a single line into a sequence of Span objects.
// Handles: **bold**, __bold__, *italic*, _italic_, `code`, [text](url).
// Nesting is NOT supported; first match wins.
std::vector<Span> ParseInline(std::string_view line) {
  std::vector<Span> spans;
  std::string plain;

  auto flush_plain = [&]() {
    if (!plain.empty()) {
      spans.push_back({SpanKind::Plain, std::move(plain), {}});
      plain.clear();
    }
  };

  size_t i = 0;
  while (i < line.size()) {
    // Bold: **text** or __text__
    if (i + 1 < line.size() &&
        ((line[i] == '*' && line[i + 1] == '*') ||
         (line[i] == '_' && line[i + 1] == '_'))) {
      char delim = line[i];
      // Search for the closing delimiter pair
      std::string close_pat = {delim, delim};
      size_t end = line.find(close_pat, i + 2);
      if (end != std::string_view::npos) {
        flush_plain();
        spans.push_back(
            {SpanKind::Bold, std::string(line.substr(i + 2, end - i - 2)), {}});
        i = end + 2;
        continue;
      }
    }

    // Italic: *text* or _text_
    if ((line[i] == '*' || line[i] == '_') &&
        (i == 0 || line[i - 1] != line[i])) {
      char delim = line[i];
      // Make sure it's not immediately another delimiter (which would be bold)
      if (i + 1 < line.size() && line[i + 1] != delim) {
        size_t end = line.find(delim, i + 1);
        if (end != std::string_view::npos && end != i + 1) {
          flush_plain();
          spans.push_back({SpanKind::Italic,
                           std::string(line.substr(i + 1, end - i - 1)), {}});
          i = end + 1;
          continue;
        }
      }
    }

    // Inline code: `code`
    if (line[i] == '`') {
      size_t end = line.find('`', i + 1);
      if (end != std::string_view::npos) {
        flush_plain();
        spans.push_back(
            {SpanKind::Code, std::string(line.substr(i + 1, end - i - 1)), {}});
        i = end + 1;
        continue;
      }
    }

    // Link: [text](url)
    if (line[i] == '[') {
      size_t close_bracket = line.find(']', i + 1);
      if (close_bracket != std::string_view::npos &&
          close_bracket + 1 < line.size() &&
          line[close_bracket + 1] == '(') {
        size_t close_paren = line.find(')', close_bracket + 2);
        if (close_paren != std::string_view::npos) {
          flush_plain();
          std::string link_text =
              std::string(line.substr(i + 1, close_bracket - i - 1));
          std::string url = std::string(
              line.substr(close_bracket + 2, close_paren - close_bracket - 2));
          spans.push_back({SpanKind::Link, std::move(link_text), std::move(url)});
          i = close_paren + 1;
          continue;
        }
      }
    }

    plain += line[i];
    ++i;
  }

  flush_plain();
  return spans;
}

// ─── Render a single inline span as an Element ───────────────────────────────

Element RenderSpan(const Span& span) {
  const Theme& t = GetTheme();
  switch (span.kind) {
    case SpanKind::Plain:
      return text(span.text);
    case SpanKind::Bold:
      return text(span.text) | bold;
    case SpanKind::Italic:
      return text(span.text) | dim;
    case SpanKind::Code:
      return text(" " + span.text + " ") | color(Color::GreenLight) | inverted;
    case SpanKind::Link:
      return hbox({
          text(span.text) | bold | color(Color(100, 150, 255)),
          text(" ↗") | dim | color(t.text_muted),
      });
  }
  return text(span.text);
}

// ─── Render a paragraph line with inline formatting ──────────────────────────

Element RenderInline(std::string_view line) {
  auto spans = ParseInline(line);
  if (spans.empty()) {
    return text("");
  }
  Elements elems;
  elems.reserve(spans.size());
  for (const auto& s : spans) {
    elems.push_back(RenderSpan(s));
  }
  if (elems.size() == 1) {
    return elems[0];
  }
  return hbox(std::move(elems));
}

// ─── Trim leading/trailing whitespace ────────────────────────────────────────

std::string_view Trim(std::string_view s) {
  size_t start = 0;
  while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) {
    ++start;
  }
  size_t end = s.size();
  while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(start, end - start);
}

// ─── Check if a line is a horizontal rule (---, ___, ***) ────────────────────

bool IsHorizontalRule(std::string_view line) {
  std::string_view trimmed = Trim(line);
  if (trimmed.size() < 3) return false;
  char c = trimmed[0];
  if (c != '-' && c != '_' && c != '*') return false;
  for (char ch : trimmed) {
    if (ch != c && ch != ' ') return false;
  }
  size_t count = 0;
  for (char ch : trimmed) {
    if (ch == c) ++count;
  }
  return count >= 3;
}

// ─── Split string into lines ─────────────────────────────────────────────────

std::vector<std::string_view> SplitLines(std::string_view text) {
  std::vector<std::string_view> lines;
  size_t start = 0;
  for (size_t i = 0; i <= text.size(); ++i) {
    if (i == text.size() || text[i] == '\n') {
      lines.push_back(text.substr(start, i - start));
      start = i + 1;
    }
  }
  return lines;
}

// ─── Main Markdown renderer ───────────────────────────────────────────────────

Elements RenderMarkdown(std::string_view md) {
  const Theme& t = GetTheme();
  Elements result;

  auto lines = SplitLines(md);
  bool in_code_block = false;
  std::string code_lang;
  std::vector<std::string> code_lines;

  auto flush_code_block = [&]() {
    if (code_lines.empty()) {
      result.push_back(emptyElement());
      return;
    }
    Elements block_lines;
    block_lines.reserve(code_lines.size());
    for (const auto& cl : code_lines) {
      block_lines.push_back(text(cl) | dim);
    }
    result.push_back(vbox(std::move(block_lines)) |
                     borderStyled(ROUNDED, t.border_color));
    code_lines.clear();
    code_lang.clear();
  };

  for (size_t idx = 0; idx < lines.size(); ++idx) {
    std::string_view raw = lines[idx];
    // Preserve raw line for code blocks, but trim trailing \r
    std::string_view line = raw;
    if (!line.empty() && line.back() == '\r') {
      line = line.substr(0, line.size() - 1);
    }

    // ── Code block fence ─────────────────────────────────────────────────────
    if (line.size() >= 3 && line.substr(0, 3) == "```") {
      if (!in_code_block) {
        in_code_block = true;
        code_lang = std::string(Trim(line.substr(3)));
      } else {
        in_code_block = false;
        flush_code_block();
      }
      continue;
    }

    if (in_code_block) {
      code_lines.push_back(std::string(line));
      continue;
    }

    std::string_view trimmed = Trim(line);

    // ── Blank line ────────────────────────────────────────────────────────────
    if (trimmed.empty()) {
      result.push_back(text(""));
      continue;
    }

    // ── Horizontal rule ───────────────────────────────────────────────────────
    if (IsHorizontalRule(trimmed)) {
      result.push_back(separator());
      continue;
    }

    // ── Headings ──────────────────────────────────────────────────────────────
    if (trimmed.size() >= 2 && trimmed[0] == '#') {
      int level = 0;
      while (level < static_cast<int>(trimmed.size()) && trimmed[level] == '#') {
        ++level;
      }
      std::string_view heading_text = Trim(trimmed.substr(level));

      if (level == 1) {
        result.push_back(text(std::string(heading_text)) | bold |
                         color(t.primary));
        result.push_back(separator());
      } else if (level == 2) {
        result.push_back(text(std::string(heading_text)) | bold |
                         color(t.accent));
      } else {
        // H3+
        result.push_back(text(std::string(heading_text)) | bold | dim);
      }
      continue;
    }

    // ── Blockquote ────────────────────────────────────────────────────────────
    if (trimmed.size() >= 2 && trimmed[0] == '>' && trimmed[1] == ' ') {
      std::string_view content = Trim(trimmed.substr(2));
      result.push_back(
          hbox({
              text("  ┃ ") | color(t.text_muted),
              RenderInline(content) | color(t.text_muted),
          }) |
          dim);
      continue;
    }
    if (!trimmed.empty() && trimmed[0] == '>') {
      std::string_view content = Trim(trimmed.substr(1));
      result.push_back(
          hbox({
              text("  ┃ ") | color(t.text_muted),
              RenderInline(content) | color(t.text_muted),
          }) |
          dim);
      continue;
    }

    // ── Unordered list ────────────────────────────────────────────────────────
    if (trimmed.size() >= 2 &&
        (trimmed[0] == '-' || trimmed[0] == '*' || trimmed[0] == '+') &&
        trimmed[1] == ' ') {
      std::string_view content = Trim(trimmed.substr(2));
      result.push_back(hbox({
          text("  • "),
          RenderInline(content) | flex,
      }));
      continue;
    }

    // ── Ordered list ─────────────────────────────────────────────────────────
    {
      size_t dot = trimmed.find(". ");
      if (dot != std::string_view::npos && dot > 0 && dot < 5) {
        bool all_digits = true;
        for (size_t k = 0; k < dot; ++k) {
          if (!std::isdigit(static_cast<unsigned char>(trimmed[k]))) {
            all_digits = false;
            break;
          }
        }
        if (all_digits) {
          std::string_view num = trimmed.substr(0, dot);
          std::string_view content = Trim(trimmed.substr(dot + 2));
          result.push_back(hbox({
              text("  " + std::string(num) + ". "),
              RenderInline(content) | flex,
          }));
          continue;
        }
      }
    }

    // ── Plain paragraph ───────────────────────────────────────────────────────
    result.push_back(RenderInline(trimmed) | flex);
  }

  // Flush any unclosed code block
  if (in_code_block) {
    flush_code_block();
  }

  if (result.empty()) {
    result.push_back(text(""));
  }

  return result;
}

}  // namespace

// ─── Public API ──────────────────────────────────────────────────────────────

ftxui::Element Markdown(std::string_view md) {
  return vbox(RenderMarkdown(md));
}

ftxui::Element Markdown(std::string_view md, int /*width*/) {
  // width is reserved for future word-wrap support
  return vbox(RenderMarkdown(md));
}

ftxui::Component MarkdownComponent(std::string_view md) {
  // Capture content by value so the component owns it
  std::string content(md);

  struct State {
    int scroll = 0;
  };
  auto state = std::make_shared<State>();

  auto renderer = Renderer([content, state]() -> Element {
    Element doc = Markdown(content);
    return doc | yframe | vscroll_indicator | flex |
           focusPosition(0, state->scroll);
  });

  renderer |= CatchEvent([state](Event event) -> bool {
    if (event == Event::ArrowDown) {
      state->scroll++;
      return true;
    }
    if (event == Event::ArrowUp) {
      state->scroll = std::max(0, state->scroll - 1);
      return true;
    }
    if (event == Event::PageDown) {
      state->scroll += 10;
      return true;
    }
    if (event == Event::PageUp) {
      state->scroll = std::max(0, state->scroll - 10);
      return true;
    }
    if (event == Event::Home) {
      state->scroll = 0;
      return true;
    }
    return false;
  });

  return renderer;
}

}  // namespace ftxui::ui
