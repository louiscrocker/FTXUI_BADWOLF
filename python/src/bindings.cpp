// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <memory>
#include <string>

#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include "ftxui/ui/app.hpp"
#include "ftxui/ui/log_panel.hpp"
#include "ftxui/ui/markdown.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/theme.hpp"
#include "ftxui/util/ref.hpp"

namespace py = pybind11;

// ---------------------------------------------------------------------------
// Reactive<T> wrappers
// Reactive<T> is non-copyable/non-movable, so we manage it via shared_ptr.
// ---------------------------------------------------------------------------

template <typename T>
struct ReactiveWrapper {
  explicit ReactiveWrapper(T init)
      : ptr(std::make_shared<ftxui::ui::Reactive<T>>(std::move(init))) {}

  T get() const { return ptr->Get(); }

  void set(T val) { ptr->Set(std::move(val)); }

  int on_change(py::function fn) {
    return ptr->OnChange([fn](const T& v) {
      py::gil_scoped_acquire acq;
      fn(v);
    });
  }

  void remove_listener(int id) { ptr->RemoveListener(id); }

  void notify() { ptr->Notify(); }

  std::shared_ptr<ftxui::ui::Reactive<T>> ptr;
};

using ReactiveIntWrapper = ReactiveWrapper<int>;
using ReactiveFloatWrapper = ReactiveWrapper<float>;
using ReactiveStrWrapper = ReactiveWrapper<std::string>;

// ---------------------------------------------------------------------------
// Bindings
// ---------------------------------------------------------------------------

PYBIND11_MODULE(_ftxui, m) {
  m.doc() =
      "FTXUI Python bindings — wraps ftxui::ui, dom, and component layers";

  // ── Opaque element/component types ────────────────────────────────────────
  // Element = std::shared_ptr<Node>, Component = std::shared_ptr<ComponentBase>
  py::class_<ftxui::Node, std::shared_ptr<ftxui::Node>>(m, "Element");
  py::class_<ftxui::ComponentBase, std::shared_ptr<ftxui::ComponentBase>>(
      m, "Component")
      .def("Render", [](ftxui::ComponentBase& self) -> ftxui::Element {
        return self.Render();
      }, "Render this component to an Element for use in a DOM tree.")
      .def("render", [](ftxui::ComponentBase& self) -> ftxui::Element {
        return self.Render();
      }, "Alias for Render().");

  // ── DOM — layout ──────────────────────────────────────────────────────────
  m.def("hbox",
        [](std::vector<ftxui::Element> els) { return ftxui::hbox(els); });
  m.def("vbox",
        [](std::vector<ftxui::Element> els) { return ftxui::vbox(els); });
  m.def("dbox",
        [](std::vector<ftxui::Element> els) { return ftxui::dbox(els); });

  // ── DOM — text ────────────────────────────────────────────────────────────
  m.def("_text_raw", [](std::string s) { return ftxui::text(s); });
  m.def("_bold", [](ftxui::Element e) { return ftxui::bold(std::move(e)); });
  m.def("_dim", [](ftxui::Element e) { return ftxui::dim(std::move(e)); });
  m.def("_italic",
        [](ftxui::Element e) { return ftxui::italic(std::move(e)); });
  m.def("_inverted",
        [](ftxui::Element e) { return ftxui::inverted(std::move(e)); });
  m.def("paragraph", [](std::string s) { return ftxui::paragraph(s); });
  m.def("vtext", [](std::string s) { return ftxui::vtext(s); });

  // ── DOM — borders ─────────────────────────────────────────────────────────
  m.def("border", [](ftxui::Element e) { return ftxui::border(std::move(e)); });
  m.def("border_rounded",
        [](ftxui::Element e) { return ftxui::borderRounded(std::move(e)); });
  m.def("border_light",
        [](ftxui::Element e) { return ftxui::borderLight(std::move(e)); });
  m.def("border_heavy",
        [](ftxui::Element e) { return ftxui::borderHeavy(std::move(e)); });
  m.def("window", [](ftxui::Element title, ftxui::Element content) {
    return ftxui::window(std::move(title), std::move(content));
  });

  // ── DOM — separators ──────────────────────────────────────────────────────
  m.def("separator", []() { return ftxui::separator(); });
  m.def("separator_light", []() { return ftxui::separatorLight(); });
  m.def("separator_heavy", []() { return ftxui::separatorHeavy(); });
  m.def("empty_element", []() { return ftxui::emptyElement(); });

  // ── DOM — sizing / flex ───────────────────────────────────────────────────
  m.def("flex", [](ftxui::Element e) { return ftxui::flex(std::move(e)); });
  m.def("flex_grow",
        [](ftxui::Element e) { return ftxui::flex_grow(std::move(e)); });
  m.def("xflex", [](ftxui::Element e) { return ftxui::xflex(std::move(e)); });
  m.def("yflex", [](ftxui::Element e) { return ftxui::yflex(std::move(e)); });
  m.def("notflex",
        [](ftxui::Element e) { return ftxui::notflex(std::move(e)); });

  // ── DOM — misc ────────────────────────────────────────────────────────────
  m.def(
      "spinner",
      [](int idx, size_t frame) { return ftxui::spinner(idx, frame); },
      py::arg("charset_index"), py::arg("frame_index") = size_t(0));
  m.def("gauge", [](float v) { return ftxui::gauge(v); });
  m.def("gauge_left", [](float v) { return ftxui::gaugeLeft(v); });
  m.def("gauge_right", [](float v) { return ftxui::gaugeRight(v); });

  // ── DOM — color ───────────────────────────────────────────────────────────
  // Register Color as a pybind11 class so instances can cross the boundary.
  py::class_<ftxui::Color>(m, "Color")
      .def(py::init<uint8_t, uint8_t, uint8_t>(), py::arg("r"), py::arg("g"),
           py::arg("b"))
      .def_static("rgb", [](uint8_t r, uint8_t g,
                            uint8_t b) { return ftxui::Color::RGB(r, g, b); })
      .def_static("hsv", [](uint8_t h, uint8_t s, uint8_t v) {
        return ftxui::Color::HSV(h, s, v);
      });

  // Named color constants — explicitly construct Color objects so pybind11
  // never sees the raw Palette1/Palette16 enum values.
  m.attr("COLOR_DEFAULT") = ftxui::Color(ftxui::Color::Default);
  m.attr("COLOR_BLACK") = ftxui::Color(ftxui::Color::Black);
  m.attr("COLOR_WHITE") = ftxui::Color(ftxui::Color::White);
  m.attr("COLOR_RED") = ftxui::Color(ftxui::Color::Red);
  m.attr("COLOR_GREEN") = ftxui::Color(ftxui::Color::Green);
  m.attr("COLOR_BLUE") = ftxui::Color(ftxui::Color::Blue);
  m.attr("COLOR_CYAN") = ftxui::Color(ftxui::Color::Cyan);
  m.attr("COLOR_YELLOW") = ftxui::Color(ftxui::Color::Yellow);
  m.attr("COLOR_MAGENTA") = ftxui::Color(ftxui::Color::Magenta);
  m.attr("COLOR_GRAY_LIGHT") = ftxui::Color(ftxui::Color::GrayLight);
  m.attr("COLOR_GRAY_DARK") = ftxui::Color(ftxui::Color::GrayDark);

  m.def("_color_element", [](ftxui::Color c, ftxui::Element e) {
    return ftxui::color(c, std::move(e));
  });
  m.def("_bgcolor_element", [](ftxui::Color c, ftxui::Element e) {
    return ftxui::bgcolor(c, std::move(e));
  });

  // ── Components — Renderer ─────────────────────────────────────────────────
  m.def("make_renderer", [](py::function render_fn) -> ftxui::Component {
    return ftxui::Renderer([render_fn]() -> ftxui::Element {
      py::gil_scoped_acquire acq;
      return render_fn().cast<ftxui::Element>();
    });
  });

  // ── Components — Button ───────────────────────────────────────────────────
  m.def("make_button",
        [](std::string label, py::function on_click) -> ftxui::Component {
          return ftxui::Button(label, [on_click]() {
            py::gil_scoped_acquire acq;
            on_click();
          });
        });

  m.def("make_button_animated",
        [](std::string label, py::function on_click) -> ftxui::Component {
          return ftxui::Button(
              label,
              [on_click]() {
                py::gil_scoped_acquire acq;
                on_click();
              },
              ftxui::ButtonOption::Animated());
        });

  // ── Components — Input ────────────────────────────────────────────────────
  // Returns (Component, get_fn, set_fn) tuple.
  // content_storage keeps the std::string alive as long as on_change closure.
  m.def(
      "make_input",
      [](std::string placeholder,
         py::object value_ref) -> std::pair<ftxui::Component, py::object> {
        auto content = std::make_shared<std::string>(
            value_ref.is_none() ? std::string("")
                                : value_ref.attr("get")().cast<std::string>());

        ftxui::InputOption opt = ftxui::InputOption::Default();
        opt.content = ftxui::StringRef(content.get());
        opt.placeholder = placeholder;
        opt.on_change = [content, value_ref]() {
          if (!value_ref.is_none()) {
            py::gil_scoped_acquire acq;
            value_ref.attr("set")(*content);
          }
        };

        auto component = ftxui::Input(opt);
        auto get_fn =
            py::cpp_function([content]() -> std::string { return *content; });
        return {component, get_fn};
      },
      py::arg("placeholder") = "", py::arg("value") = py::none());

  // ── Components — Checkbox ─────────────────────────────────────────────────
  // Returns (Component, get_fn, set_fn) tuple.
  m.def(
      "make_checkbox",
      [](std::string label, bool init, py::object on_change) -> py::tuple {
        auto checked = std::make_shared<bool>(init);

        ftxui::CheckboxOption opt = ftxui::CheckboxOption::Simple();
        opt.label = label;
        opt.checked = ftxui::Ref<bool>(checked.get());
        if (!on_change.is_none()) {
          opt.on_change = [checked, on_change]() {
            py::gil_scoped_acquire acq;
            on_change(*checked);
          };
        }

        auto component = ftxui::Checkbox(opt);
        auto get_fn = py::cpp_function([checked]() { return *checked; });
        auto set_fn = py::cpp_function([checked](bool v) { *checked = v; });
        return py::make_tuple(component, get_fn, set_fn);
      },
      py::arg("label"), py::arg("checked") = false,
      py::arg("on_change") = py::none());

  // ── Components — Containers ───────────────────────────────────────────────
  m.def("container_vertical",
        [](std::vector<ftxui::Component> comps) -> ftxui::Component {
          return ftxui::Container::Vertical(comps);
        });
  m.def("container_horizontal",
        [](std::vector<ftxui::Component> comps) -> ftxui::Component {
          return ftxui::Container::Horizontal(comps);
        });

  // ── App control ───────────────────────────────────────────────────────────
  m.def("run_fullscreen", [](ftxui::Component c) {
    py::gil_scoped_release release;
    ftxui::ui::RunFullscreen(std::move(c));
  });
  m.def("run", [](ftxui::Component c) {
    py::gil_scoped_release release;
    ftxui::ui::Run(std::move(c));
  });
  m.def("run_fit_component", [](ftxui::Component c) {
    py::gil_scoped_release release;
    ftxui::ui::RunFitComponent(std::move(c));
  });
  m.def("app_exit", []() {
    if (ftxui::App* app = ftxui::App::Active()) {
      app->Exit();
    }
  });
  m.def("app_post_event", []() {
    if (ftxui::App* app = ftxui::App::Active()) {
      app->PostEvent(ftxui::Event::Custom);
    }
  });

  // ── Theme ─────────────────────────────────────────────────────────────────
  py::class_<ftxui::ui::Theme>(m, "Theme")
      .def(py::init<>())
      .def_static("default_theme", &ftxui::ui::Theme::Default)
      .def_static("dark", &ftxui::ui::Theme::Dark)
      .def_static("light", &ftxui::ui::Theme::Light)
      .def_static("nord", &ftxui::ui::Theme::Nord)
      .def_static("dracula", &ftxui::ui::Theme::Dracula)
      .def_static("monokai", &ftxui::ui::Theme::Monokai);

  m.def("set_theme", &ftxui::ui::SetTheme);
  m.def("get_theme", &ftxui::ui::GetTheme, py::return_value_policy::reference);

  // ── Reactive<int> ─────────────────────────────────────────────────────────
  py::class_<ReactiveIntWrapper, std::shared_ptr<ReactiveIntWrapper>>(
      m, "ReactiveInt")
      .def(py::init<int>(), py::arg("initial") = 0)
      .def("get", &ReactiveIntWrapper::get)
      .def("set", &ReactiveIntWrapper::set)
      .def("on_change", &ReactiveIntWrapper::on_change)
      .def("remove_listener", &ReactiveIntWrapper::remove_listener)
      .def("notify", &ReactiveIntWrapper::notify);

  // ── Reactive<float> ───────────────────────────────────────────────────────
  py::class_<ReactiveFloatWrapper, std::shared_ptr<ReactiveFloatWrapper>>(
      m, "ReactiveFloat")
      .def(py::init<float>(), py::arg("initial") = 0.0f)
      .def("get", &ReactiveFloatWrapper::get)
      .def("set", &ReactiveFloatWrapper::set)
      .def("on_change", &ReactiveFloatWrapper::on_change)
      .def("remove_listener", &ReactiveFloatWrapper::remove_listener)
      .def("notify", &ReactiveFloatWrapper::notify);

  // ── Reactive<std::string> ─────────────────────────────────────────────────
  py::class_<ReactiveStrWrapper, std::shared_ptr<ReactiveStrWrapper>>(
      m, "ReactiveStr")
      .def(py::init<std::string>(), py::arg("initial") = "")
      .def("get", &ReactiveStrWrapper::get)
      .def("set", &ReactiveStrWrapper::set)
      .def("on_change", &ReactiveStrWrapper::on_change)
      .def("remove_listener", &ReactiveStrWrapper::remove_listener)
      .def("notify", &ReactiveStrWrapper::notify);

  // ── LogPanel ──────────────────────────────────────────────────────────────
  py::enum_<ftxui::ui::LogLevel>(m, "LogLevel")
      .value("TRACE", ftxui::ui::LogLevel::Trace)
      .value("DEBUG", ftxui::ui::LogLevel::Debug)
      .value("INFO", ftxui::ui::LogLevel::Info)
      .value("WARN", ftxui::ui::LogLevel::Warn)
      .value("ERROR", ftxui::ui::LogLevel::Error);

  py::class_<ftxui::ui::LogPanel, std::shared_ptr<ftxui::ui::LogPanel>>(
      m, "LogPanel")
      .def_static("create", &ftxui::ui::LogPanel::Create,
                  py::arg("max_lines") = size_t(1000))
      .def(
          "log",
          [](ftxui::ui::LogPanel& self, std::string msg,
             ftxui::ui::LogLevel lvl) { self.Log(msg, lvl); },
          py::arg("message"), py::arg("level") = ftxui::ui::LogLevel::Info)
      .def("trace",
           [](ftxui::ui::LogPanel& self, std::string msg) { self.Trace(msg); })
      .def("debug",
           [](ftxui::ui::LogPanel& self, std::string msg) { self.Debug(msg); })
      .def("info",
           [](ftxui::ui::LogPanel& self, std::string msg) { self.Info(msg); })
      .def("warn",
           [](ftxui::ui::LogPanel& self, std::string msg) { self.Warn(msg); })
      .def("error",
           [](ftxui::ui::LogPanel& self, std::string msg) { self.Error(msg); })
      .def("clear", &ftxui::ui::LogPanel::Clear)
      .def("set_max_lines", &ftxui::ui::LogPanel::SetMaxLines)
      .def("set_min_level", &ftxui::ui::LogPanel::SetMinLevel)
      .def(
          "build",
          [](ftxui::ui::LogPanel& self, std::string title) {
            return self.Build(title);
          },
          py::arg("title") = "");

  // ── Markdown ──────────────────────────────────────────────────────────────
  m.def("markdown", [](std::string md) { return ftxui::ui::Markdown(md); });
  m.def("markdown_with_width", [](std::string md, int width) {
    return ftxui::ui::Markdown(md, width);
  });
  m.def("markdown_component",
        [](std::string md) { return ftxui::ui::MarkdownComponent(md); });
}
