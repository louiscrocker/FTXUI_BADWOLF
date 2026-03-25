// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_FWD_HPP
#define FTXUI_UI_FWD_HPP

/// @brief Forward declarations for ftxui::ui types.
///
/// Include this header in other headers that only need type names, not full
/// definitions.  Include "ftxui/ui.hpp" in .cpp files for full definitions.

#include <functional>
#include <memory>
#include <string>

// ── Core ftxui forward declarations ──────────────────────────────────────────
namespace ftxui {
class ComponentBase;
using Component = std::shared_ptr<ComponentBase>;
class Node;
using Element = std::shared_ptr<Node>;
}  // namespace ftxui

// ── ftxui::ui forward declarations ───────────────────────────────────────────
namespace ftxui::ui {

// Core
struct Theme;
template <typename T>
class State;
template <typename T>
class Reactive;
template <typename T>
class ReactiveList;
template <typename T>
class Bind;

// Data
class JsonValue;
class JsonPath;
class ReactiveJson;

// Text
class TextStyle;
class RichText;
class AnsiParser;
struct Span;

// Console / typewriter
class Console;
struct ConsoleLine;
struct TypewriterConfig;
struct ConsolePromptConfig;

// Networking
class NetworkReactiveServer;
template <typename T>
class NetworkReactiveClient;
class CollabServer;
class CollabClient;

// Live data
template <typename T>
class LiveSource;
class HttpJsonSource;
class PrometheusSource;
class FileTailSource;

// Registry
class Registry;
struct ComponentMeta;

// LLM / AI
class NLParser;
class UIGenerator;
class LLMAdapter;
struct UIIntent;

}  // namespace ftxui::ui

#endif  // FTXUI_UI_FWD_HPP
