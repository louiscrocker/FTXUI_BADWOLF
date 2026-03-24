// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file form_demo.cpp
/// Demonstrates the ftxui::ui::Form builder API.
///
/// The form is a simple sign-up screen with multiple field types: text inputs,
/// password, checkbox, dropdown, and radio buttons.

#include <string>
#include <vector>

#include "ftxui/component/app.hpp"
#include "ftxui/ui.hpp"

using namespace ftxui;
using namespace ftxui::ui;

int main() {
  // ── Theme (optional – comment out to use Default) ──────────────────────────
  SetTheme(Theme::Nord());

  // ── Model ──────────────────────────────────────────────────────────────────
  std::string first_name;
  std::string last_name;
  std::string email;
  std::string password;
  bool subscribe = false;
  int country = 0;
  const std::vector<std::string> countries = {"USA", "UK", "Canada", "Other"};

  // ── App ────────────────────────────────────────────────────────────────────
  auto app = App::TerminalOutput();

  auto on_submit = [&] {
    // In a real app you would validate and process the data here.
    app.Exit();
  };

  // ── Form ───────────────────────────────────────────────────────────────────
  auto form = Form()
                  .Title("Create Account")
                  .LabelWidth(15)
                  .Section("Personal Info")
                  .Field("First name", &first_name, "Alice")
                  .Field("Last name", &last_name, "Smith")
                  .Field("Email", &email, "you@example.com")
                  .Section("Security")
                  .Password("Password", &password, "at least 8 chars")
                  .Section("Preferences")
                  .Select("Country", &countries, &country)
                  .Check("Subscribe to newsletter", &subscribe)
                  .Separator()
                  .Submit("  Create  ", on_submit)
                  .Cancel("  Cancel  ", [&] { app.Exit(); });

  app.Loop(form.Build());
  return 0;
}
