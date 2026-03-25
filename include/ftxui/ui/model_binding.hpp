// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#ifndef FTXUI_UI_MODEL_BINDING_HPP
#define FTXUI_UI_MODEL_BINDING_HPP

// ---------------------------------------------------------------------------
// model_binding.hpp  — struct-to-form binding helpers
// ---------------------------------------------------------------------------
//
// This header re-exports MakeLens (defined in bind.hpp) and documents the
// BindField / BindPassword / BindCheckbox methods added to Form in form.hpp.
//
// Include order:
//   #include "ftxui/ui/model_binding.hpp"   // pulls in bind.hpp + form.hpp
//
// Example:
//   struct LoginForm { std::string username, password; bool remember = false; };
//
//   auto model = MakeBind<LoginForm>();
//   auto form  = Form()
//       .Title("Login")
//       .BindField("Username", model, &LoginForm::username)
//       .BindField("Password", model, &LoginForm::password)
//       .BindCheckbox("Remember me", model, &LoginForm::remember)
//       .OnSubmit([&]{ auto d = model.Get(); /* use d.username ... */ })
//       .Build();
// ---------------------------------------------------------------------------

#include "ftxui/ui/bind.hpp"

// form.hpp provides the BindField / BindPassword / BindCheckbox template methods.
#include "ftxui/ui/form.hpp"

// Re-export MakeLens for users who only include this header.
// (MakeLens is already defined in bind.hpp.)

#endif  // FTXUI_UI_MODEL_BINDING_HPP
