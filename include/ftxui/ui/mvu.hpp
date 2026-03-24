// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_MVU_HPP
#define FTXUI_UI_MVU_HPP

#include <functional>
#include <memory>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

namespace ftxui::ui {

/// @brief Model-View-Update (Elm / Bubbletea-style) architecture adapter.
///
/// Lets you write FTXUI applications in a purely functional style:
///
/// @code
/// struct Model { int count = 0; };
/// enum Msg { Inc, Dec, Quit };
///
/// Model Update(Model m, Msg msg) {
///   switch (msg) {
///     case Inc:  m.count++; return m;
///     case Dec:  m.count--; return m;
///     case Quit: App::Active()->Exit(); return m;
///   }
///   return m;
/// }
///
/// Element View(const Model& m, std::function<void(Msg)> dispatch) {
///   return vbox({
///       text("Count: " + std::to_string(m.count)) | bold | center,
///       hbox({
///           Button("−", [&]{ dispatch(Dec); }) -> Render(),
///           Button("+", [&]{ dispatch(Inc); }) -> Render(),
///           Button("q", [&]{ dispatch(Quit); }) -> Render(),
///       }) | center,
///   }) | border | center;
/// }
///
/// int main() {
///   ftxui::ui::MVU<Model, Msg>({}, Update, View).Run();
/// }
/// @endcode
///
/// @tparam TModel  Plain-old-data model (copied on every Update call).
/// @tparam TMsg    Message type (typically an enum or a std::variant).
///
/// @ingroup ui
template <typename TModel, typename TMsg>
class MVU {
 public:
  using UpdateFn =
      std::function<TModel(TModel, TMsg)>;  ///< Pure reducer function.
  using ViewFn = std::function<Element(  ///< Render function.
      const TModel&,
      std::function<void(TMsg)>  ///< dispatch — send a message
      )>;

  /// @brief Construct an MVU app.
  /// @param initial  Starting state of the model.
  /// @param update   Pure reducer: (model, msg) → new model.
  /// @param view     Pure renderer: (model, dispatch) → Element.
  MVU(TModel initial, UpdateFn update, ViewFn view)
      : model_(std::make_shared<TModel>(std::move(initial))),
        update_(std::move(update)),
        view_(std::move(view)) {}

  /// @brief Build the root Component that drives the MVU loop.
  ///
  /// The returned component can be handed to any App::Loop() call.
  Component Build() {
    auto model = model_;
    auto update = update_;
    auto view = view_;

    // dispatch is captured by the Renderer lambda; calling it from a button
    // on_click or a CatchEvent handler is safe because those are invoked from
    // within the App event loop.
    auto dispatch = [model, update](TMsg msg) {
      *model = update(*model, msg);
      if (App* app = App::Active()) {
        app->PostEvent(Event::Custom);
      }
    };

    return Renderer([model, view, dispatch]() -> Element {
      return view(*model, dispatch);
    });
  }

  /// @brief Run the app on the terminal output (scrolling, no alt-screen).
  void Run() {
    auto app = App::TerminalOutput();
    app.Loop(Build());
  }

  /// @brief Run the app in fullscreen (alternate screen buffer).
  void RunFullscreen() {
    auto app = App::Fullscreen();
    app.Loop(Build());
  }

  /// @brief Run the app sized to fit the rendered component.
  void RunFitComponent() {
    auto app = App::FitComponent();
    app.Loop(Build());
  }

 private:
  std::shared_ptr<TModel> model_;
  UpdateFn update_;
  ViewFn view_;
};

// ── Convenience free functions ────────────────────────────────────────────────

/// @brief Run an MVU app on the terminal output.
template <typename TModel, typename TMsg>
void Run(
    TModel initial,
    std::function<TModel(TModel, TMsg)> update,
    std::function<Element(const TModel&, std::function<void(TMsg)>)> view) {
  MVU<TModel, TMsg>(std::move(initial), std::move(update), std::move(view))
      .Run();
}

/// @brief Run an MVU app in fullscreen.
template <typename TModel, typename TMsg>
void RunFullscreen(
    TModel initial,
    std::function<TModel(TModel, TMsg)> update,
    std::function<Element(const TModel&, std::function<void(TMsg)>)> view) {
  MVU<TModel, TMsg>(std::move(initial), std::move(update), std::move(view))
      .RunFullscreen();
}

}  // namespace ftxui::ui

#endif  // FTXUI_UI_MVU_HPP
