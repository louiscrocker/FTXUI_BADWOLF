// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/registry.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace ftxui::ui {

namespace {

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

bool ContainsIgnoreCase(const std::string& haystack,
                        const std::string& needle) {
  return ToLower(haystack).find(ToLower(needle)) != std::string::npos;
}

}  // namespace

// ── Registry ─────────────────────────────────────────────────────────────────

Registry& Registry::Get() {
  static Registry instance;
  return instance;
}

void Registry::Register(ComponentMeta meta,
                        std::function<ftxui::Component()> factory) {
  // Silently replace an existing entry with the same name so that hot-reload
  // style patterns work without duplicates.
  for (auto& e : entries_) {
    if (e.meta.name == meta.name) {
      e.meta = std::move(meta);
      e.factory = std::move(factory);
      return;
    }
  }
  entries_.push_back({std::move(meta), std::move(factory)});
}

std::vector<ComponentMeta> Registry::List() const {
  std::vector<ComponentMeta> result;
  result.reserve(entries_.size());
  for (const auto& e : entries_) {
    result.push_back(e.meta);
  }
  return result;
}

std::vector<ComponentMeta> Registry::Search(const std::string& query) const {
  std::vector<ComponentMeta> result;
  for (const auto& e : entries_) {
    bool match = ContainsIgnoreCase(e.meta.name, query) ||
                 ContainsIgnoreCase(e.meta.description, query) ||
                 ContainsIgnoreCase(e.meta.author, query);
    if (!match) {
      for (const auto& tag : e.meta.tags) {
        if (ContainsIgnoreCase(tag, query)) {
          match = true;
          break;
        }
      }
    }
    if (match) {
      result.push_back(e.meta);
    }
  }
  return result;
}

ftxui::Component Registry::Create(const std::string& name) const {
  for (const auto& e : entries_) {
    if (e.meta.name == name) {
      return e.factory();
    }
  }
  return nullptr;
}

// ── ComponentRegistrar
// ────────────────────────────────────────────────────────

ComponentRegistrar::ComponentRegistrar(
    ComponentMeta meta,
    std::function<ftxui::Component()> factory) {
  Registry::Get().Register(std::move(meta), std::move(factory));
}

}  // namespace ftxui::ui
