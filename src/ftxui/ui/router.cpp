// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/router.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_base.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/dom/elements.hpp"

using namespace ftxui;

namespace ftxui::ui {

// ── Impl ──────────────────────────────────────────────────────────────────────

struct Router::Impl {
  using FactoryFn = std::function<Component()>;
  using RouteEntry = std::variant<Component, FactoryFn>;

  std::map<std::string, RouteEntry> routes;
  std::map<std::string, Component> cache;  // Lazy-created factory cache.
  std::vector<std::string> history;
  std::string default_route;
  std::function<void(std::string_view, std::string_view)> on_navigate;

  // Transition progress: 0.0 = just started, 1.0 = complete.
  std::atomic<double> transition_progress_{1.0};

  Component GetComponent(const std::string& name) {
    auto it = routes.find(name);
    if (it == routes.end()) {
      return nullptr;
    }
    if (std::holds_alternative<Component>(it->second)) {
      return std::get<Component>(it->second);
    }
    // Lazy factory: create once and cache.
    auto& cached = cache[name];
    if (!cached) {
      cached = std::get<FactoryFn>(it->second)();
    }
    return cached;
  }

  Component CurrentComponent() {
    if (history.empty()) {
      return nullptr;
    }
    return GetComponent(history.back());
  }

  bool HasRoute(std::string_view name) const {
    return routes.count(std::string(name)) > 0;
  }

  void Navigate(std::string_view to) {
    std::string from = history.empty() ? "" : history.back();
    history.push_back(std::string(to));
    if (on_navigate) {
      on_navigate(from, to);
    }

    if (auto* app = App::Active()) {
      // Kick off a short fade-in transition: dim for ~75 ms then clear.
      transition_progress_.store(0.0);
      app->PostEvent(Event::Custom);

      std::thread([app_ptr = app, &prog = transition_progress_]() {
        constexpr int kSteps = 10;
        constexpr int kStepMs = 15;
        for (int i = 1; i <= kSteps; ++i) {
          std::this_thread::sleep_for(std::chrono::milliseconds(kStepMs));
          prog.store(static_cast<double>(i) / kSteps);
          app_ptr->PostEvent(Event::Custom);
        }
        prog.store(1.0);
      }).detach();
    }
  }
};

// ── RouterComponent ───────────────────────────────────────────────────────────

namespace {

class RouterComponent : public ComponentBase {
 public:
  explicit RouterComponent(std::shared_ptr<Router::Impl> impl)
      : impl_(std::move(impl)) {}

  Element OnRender() override {
    auto child = impl_->CurrentComponent();
    if (!child) {
      return text("(no route)");
    }
    Element render = child->Render();
    // Apply a brief dim effect during the first half of the transition.
    if (impl_->transition_progress_.load() < 0.5) {
      render = render | dim;
    }
    return render;
  }

  bool OnEvent(Event event) override {
    auto child = impl_->CurrentComponent();
    return child ? child->OnEvent(event) : false;
  }

  Component ActiveChild() override { return impl_->CurrentComponent(); }

 private:
  std::shared_ptr<Router::Impl> impl_;
};

}  // namespace

// ── Router ────────────────────────────────────────────────────────────────────

Router::Router() : impl_(std::make_shared<Impl>()) {}
Router::~Router() = default;

Router& Router::Route(std::string_view name, ftxui::Component component) {
  impl_->routes[std::string(name)] = std::move(component);
  return *this;
}

Router& Router::Route(std::string_view name,
                      std::function<ftxui::Component()> factory) {
  impl_->routes[std::string(name)] = std::move(factory);
  return *this;
}

Router& Router::Default(std::string_view name) {
  impl_->default_route = std::string(name);
  return *this;
}

Router& Router::OnNavigate(
    std::function<void(std::string_view from, std::string_view to)> cb) {
  impl_->on_navigate = std::move(cb);
  return *this;
}

void Router::Push(std::string_view name) {
  if (!impl_->HasRoute(name)) {
    return;
  }
  impl_->Navigate(name);
}

void Router::Pop() {
  if (impl_->history.size() <= 1) {
    return;
  }
  std::string from = impl_->history.back();
  impl_->history.pop_back();
  std::string to = impl_->history.empty() ? "" : impl_->history.back();
  if (impl_->on_navigate) {
    impl_->on_navigate(from, to);
  }
  if (auto* app = App::Active()) {
    app->PostEvent(Event::Custom);
  }
}

void Router::Replace(std::string_view name) {
  if (!impl_->HasRoute(name)) {
    return;
  }
  if (!impl_->history.empty()) {
    std::string from = impl_->history.back();
    impl_->history.pop_back();
    impl_->history.push_back(std::string(name));
    if (impl_->on_navigate) {
      impl_->on_navigate(from, name);
    }
    if (auto* app = App::Active()) {
      app->PostEvent(Event::Custom);
    }
  } else {
    impl_->Navigate(name);
  }
}

std::string_view Router::Current() const {
  if (impl_->history.empty()) {
    return {};
  }
  return impl_->history.back();
}

bool Router::CanGoBack() const {
  return impl_->history.size() > 1;
}

ftxui::Component Router::Build() {
  if (impl_->history.empty() && !impl_->default_route.empty()) {
    impl_->history.push_back(impl_->default_route);
  }
  return std::make_shared<RouterComponent>(impl_);
}

}  // namespace ftxui::ui
