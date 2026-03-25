// BadWolf Component: hex-viewer v1.0.0
// Install: badwolf install hex-viewer
//
// Self-contained hex dump viewer — no external dependencies beyond FTXUI.
// Displays 16 bytes per row as offset | hex columns | ASCII panel.
// Supports keyboard scrolling (↑/↓/PgUp/PgDn/Home/End).
#pragma once

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"

namespace badwolf {

/// @brief Scrollable hex dump viewer component.
///
/// Renders the given byte buffer as a classic hex editor view:
///   offset  | 16 hex bytes                           | ASCII
///   0000000 | 48 65 6c 6c 6f 2c 20 57  6f 72 6c 64  | Hello, World
///
/// Use ↑/↓ to scroll one row, PgUp/PgDn to scroll one screen, Home/End.
///
/// @param data  Byte buffer to display.
/// @returns An ftxui::Component ready to embed in any layout.
inline ftxui::Component HexViewer(const std::vector<uint8_t>& data) {
  using namespace ftxui;

  struct State {
    const std::vector<uint8_t>* buf;
    int scroll = 0;
    int visible_rows = 20;  // updated on render
    static constexpr int kBytesPerRow = 16;

    int TotalRows() const {
      return static_cast<int>(
          (buf->size() + kBytesPerRow - 1) / kBytesPerRow);
    }

    void ClampScroll() {
      scroll = std::max(0, std::min(scroll, TotalRows() - 1));
    }
  };

  auto state = std::make_shared<State>();
  // Take a copy of the data for the shared state.
  auto data_copy = std::make_shared<std::vector<uint8_t>>(data);
  state->buf = data_copy.get();

  class HexViewerComponent : public ComponentBase {
   public:
    HexViewerComponent(std::shared_ptr<State> s,
                       std::shared_ptr<std::vector<uint8_t>> d)
        : state_(std::move(s)), data_(std::move(d)) {}

    Element Render() override {
      const auto& buf = *data_;
      constexpr int kBPR = State::kBytesPerRow;

      std::vector<Element> rows;

      // Header row
      rows.push_back(hbox({
          text("  Offset  ") | bold | color(Color::GrayLight),
          text(" 00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F  ") |
              bold | color(Color::GrayLight),
          text("ASCII           ") | bold | color(Color::GrayLight),
      }));
      rows.push_back(separatorLight());

      int total_rows = state_->TotalRows();
      int visible = std::max(1, state_->visible_rows);
      int end_row = std::min(state_->scroll + visible, total_rows);

      for (int row = state_->scroll; row < end_row; ++row) {
        size_t base = static_cast<size_t>(row) * kBPR;

        // Offset
        std::ostringstream off;
        off << std::setw(8) << std::setfill('0') << std::uppercase << std::hex
            << base;
        Element offset_el = text(" " + off.str() + " ") | color(Color::Cyan);

        // Hex bytes
        std::string hex_str;
        std::string ascii_str;
        for (int col = 0; col < kBPR; ++col) {
          if (col == 8) hex_str += " ";  // extra space in the middle
          size_t idx = base + static_cast<size_t>(col);
          if (idx < buf.size()) {
            std::ostringstream h;
            h << std::setw(2) << std::setfill('0') << std::uppercase
              << std::hex << static_cast<unsigned>(buf[idx]);
            hex_str += h.str() + " ";
            unsigned char c = static_cast<unsigned char>(buf[idx]);
            ascii_str += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.';
          } else {
            hex_str += "   ";
            ascii_str += ' ';
          }
        }

        rows.push_back(hbox({
            offset_el,
            text(" ") | dim,
            text(hex_str) | color(Color::White),
            text(" │") | dim | color(Color::GrayDark),
            text(ascii_str) | color(Color::GreenLight),
            text("│") | dim | color(Color::GrayDark),
        }));
      }

      // Update visible_rows hint (approximate — will converge in 1-2 frames).
      state_->visible_rows = std::max(1, end_row - state_->scroll);

      // Status bar
      std::string status = " Rows " + std::to_string(state_->scroll + 1) +
                           "-" + std::to_string(end_row) + " / " +
                           std::to_string(total_rows) + "   " +
                           std::to_string(buf.size()) + " bytes";
      rows.push_back(separatorLight());
      rows.push_back(text(status) | dim);

      return vbox(rows) | border;
    }

    bool OnEvent(Event event) override {
      if (event == Event::ArrowUp || event == Event::Character('k')) {
        --state_->scroll;
        state_->ClampScroll();
        return true;
      }
      if (event == Event::ArrowDown || event == Event::Character('j')) {
        ++state_->scroll;
        state_->ClampScroll();
        return true;
      }
      if (event == Event::PageUp) {
        state_->scroll -= state_->visible_rows;
        state_->ClampScroll();
        return true;
      }
      if (event == Event::PageDown) {
        state_->scroll += state_->visible_rows;
        state_->ClampScroll();
        return true;
      }
      if (event == Event::Home) {
        state_->scroll = 0;
        return true;
      }
      if (event == Event::End) {
        state_->scroll = std::max(0, state_->TotalRows() - 1);
        return true;
      }
      return false;
    }

   private:
    std::shared_ptr<State> state_;
    std::shared_ptr<std::vector<uint8_t>> data_;
  };

  return std::make_shared<HexViewerComponent>(state, data_copy);
}

}  // namespace badwolf
