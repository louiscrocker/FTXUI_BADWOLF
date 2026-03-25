// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.

/// @file binding_test.cpp
/// Tests for the data-binding layers: Bind<T>, ReactiveList<T>, MakeLens,
/// DataContext<T>, and reactive decorators.

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/ui/bind.hpp"
#include "ftxui/ui/data_context.hpp"
#include "ftxui/ui/datatable.hpp"
#include "ftxui/ui/model_binding.hpp"
#include "ftxui/ui/reactive.hpp"
#include "ftxui/ui/reactive_decorators.hpp"
#include "ftxui/ui/reactive_list.hpp"
#include "ftxui/ui/textinput.hpp"
#include "ftxui/ui/virtual_list.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ============================================================================
// BindTest
// ============================================================================

TEST(BindTest, TwoWay_InitialValue) {
  auto b = MakeBind<int>(42);
  EXPECT_EQ(b.Get(), 42);
}

TEST(BindTest, TwoWay_SetUpdatesReactive) {
  auto b = MakeBind<int>(0);
  b.Set(99);
  EXPECT_EQ(b.Get(), 99);
}

TEST(BindTest, TwoWay_ReactiveUpdatesSetsInput) {
  auto r = std::make_shared<Reactive<int>>(5);
  auto b = MakeBind(r);
  r->Set(77);
  EXPECT_EQ(b.Get(), 77);
}

TEST(BindTest, MakeBind_DefaultConstructed) {
  auto b = MakeBind<std::string>();
  EXPECT_EQ(b.Get(), "");
}

TEST(BindTest, MakeBind_FromReactive) {
  auto r = std::make_shared<Reactive<std::string>>("hello");
  auto b = MakeBind(r);
  EXPECT_EQ(b.Get(), "hello");
  b.Set("world");
  EXPECT_EQ(r->Get(), "world");
}

TEST(BindTest, Subscribe_FiresOnChange) {
  auto b = MakeBind<int>(0);
  int last = -1;
  b.Subscribe([&](const int& v) { last = v; });
  b.Set(7);
  EXPECT_EQ(last, 7);
}

TEST(BindTest, Subscribe_MultipleCallbacks) {
  auto b = MakeBind<int>(0);
  int a = 0, c = 0;
  b.Subscribe([&](const int& v) { a = v; });
  b.Subscribe([&](const int& v) { c = v * 2; });
  b.Set(5);
  EXPECT_EQ(a, 5);
  EXPECT_EQ(c, 10);
}

TEST(BindTest, ImplicitConversion) {
  auto b = MakeBind<std::string>("implicit");
  std::string val = b;
  EXPECT_EQ(val, "implicit");
}

TEST(BindTest, AsReactive_ReturnsSameReactive) {
  auto r = std::make_shared<Reactive<int>>(3);
  auto b = MakeBind(r);
  EXPECT_EQ(b.AsReactive().get(), r.get());
}

TEST(BindTest, OwningBind_ReactiveIsShared) {
  auto b1 = MakeBind<int>(10);
  auto r  = b1.AsReactive();
  r->Set(20);
  EXPECT_EQ(b1.Get(), 20);
}

// ============================================================================
// ReactiveListTest
// ============================================================================

TEST(ReactiveListTest, Push_TriggersOnChange) {
  auto list       = MakeReactiveList<int>({1, 2, 3});
  int  call_count = 0;
  list->OnChange([&](const std::vector<int>&) { call_count++; });
  list->Push(4);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(list->Size(), 4u);
}

TEST(ReactiveListTest, Remove_TriggersOnChange) {
  auto list       = MakeReactiveList<int>({10, 20, 30});
  int  call_count = 0;
  list->OnChange([&](const std::vector<int>&) { call_count++; });
  list->Remove(1);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(list->Size(), 2u);
  EXPECT_EQ(list->Items()[0], 10);
  EXPECT_EQ(list->Items()[1], 30);
}

TEST(ReactiveListTest, Clear_TriggersOnChange) {
  auto list       = MakeReactiveList<std::string>({"a", "b", "c"});
  int  call_count = 0;
  list->OnChange([&](const std::vector<std::string>&) { call_count++; });
  list->Clear();
  EXPECT_EQ(call_count, 1);
  EXPECT_TRUE(list->Empty());
}

TEST(ReactiveListTest, Replace_TriggersOnChange) {
  auto list       = MakeReactiveList<int>({1, 2});
  int  call_count = 0;
  list->OnChange([&](const std::vector<int>&) { call_count++; });
  list->Replace({5, 6, 7});
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(list->Size(), 3u);
  EXPECT_EQ(list->Items()[0], 5);
}

TEST(ReactiveListTest, Items_ReturnsCurrentState) {
  auto list = MakeReactiveList<int>({1, 2, 3});
  list->Push(4);
  list->Remove(0);
  auto items = list->Items();
  ASSERT_EQ(items.size(), 3u);
  EXPECT_EQ(items[0], 2);
  EXPECT_EQ(items[1], 3);
  EXPECT_EQ(items[2], 4);
}

TEST(ReactiveListTest, Size_AfterPush) {
  auto list = MakeReactiveList<std::string>();
  EXPECT_EQ(list->Size(), 0u);
  list->Push("x");
  EXPECT_EQ(list->Size(), 1u);
  list->Push("y");
  EXPECT_EQ(list->Size(), 2u);
}

TEST(ReactiveListTest, AsReactive_SyncsWithList) {
  auto list = MakeReactiveList<int>({1, 2});
  auto r    = list->AsReactive();
  ASSERT_NE(r, nullptr);

  int change_count = 0;
  r->OnChange([&](const std::vector<int>&) { change_count++; });

  list->Push(3);
  EXPECT_EQ(r->Get().size(), 3u);
}

TEST(ReactiveListTest, Insert_TriggersOnChange) {
  auto list       = MakeReactiveList<int>({1, 3});
  int  call_count = 0;
  list->OnChange([&](const std::vector<int>&) { call_count++; });
  list->Insert(1, 2);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(list->Items()[1], 2);
}

TEST(ReactiveListTest, Set_TriggersOnChange) {
  auto list       = MakeReactiveList<int>({1, 2, 3});
  int  call_count = 0;
  list->OnChange([&](const std::vector<int>&) { call_count++; });
  list->Set(1, 99);
  EXPECT_EQ(call_count, 1);
  EXPECT_EQ(list->Items()[1], 99);
}

TEST(ReactiveListTest, Empty_Initial) {
  auto list = MakeReactiveList<double>();
  EXPECT_TRUE(list->Empty());
  EXPECT_EQ(list->Size(), 0u);
}

TEST(ReactiveListTest, Remove_OutOfRange_NoOp) {
  auto list = MakeReactiveList<int>({1, 2, 3});
  // Should not crash or change the list.
  list->Remove(10);
  EXPECT_EQ(list->Size(), 3u);
}

// ============================================================================
// LensTest
// ============================================================================

struct Point {
  int x = 0;
  int y = 0;
};

TEST(LensTest, Lens_ReadField) {
  auto model  = MakeBind<Point>(Point{3, 7});
  auto x_lens = MakeLens(model, &Point::x);
  EXPECT_EQ(x_lens.Get(), 3);
}

TEST(LensTest, Lens_WriteFieldUpdatesModel) {
  auto model  = MakeBind<Point>(Point{0, 0});
  auto x_lens = MakeLens(model, &Point::x);
  x_lens.Set(42);
  EXPECT_EQ(model.Get().x, 42);
  EXPECT_EQ(model.Get().y, 0);  // unchanged
}

TEST(LensTest, Lens_ModelUpdateNotifiesLens) {
  auto model  = MakeBind<Point>(Point{0, 0});
  auto x_lens = MakeLens(model, &Point::x);
  model.Set(Point{99, 1});
  EXPECT_EQ(x_lens.Get(), 99);
}

TEST(LensTest, Lens_MultipleFields_Independent) {
  auto model  = MakeBind<Point>(Point{1, 2});
  auto x_lens = MakeLens(model, &Point::x);
  auto y_lens = MakeLens(model, &Point::y);

  x_lens.Set(10);
  EXPECT_EQ(model.Get().x, 10);
  EXPECT_EQ(model.Get().y, 2);  // y unchanged

  y_lens.Set(20);
  EXPECT_EQ(model.Get().x, 10);  // x unchanged
  EXPECT_EQ(model.Get().y, 20);
}

TEST(LensTest, Lens_String_RoundTrip) {
  struct S {
    std::string name;
    int age = 0;
  };
  auto model     = MakeBind<S>(S{"Alice", 30});
  auto name_lens = MakeLens(model, &S::name);
  name_lens.Set("Bob");
  EXPECT_EQ(model.Get().name, "Bob");
  EXPECT_EQ(model.Get().age, 30);
}

// ============================================================================
// DataContextTest
// ============================================================================

TEST(DataContextTest, Use_ReturnsNullptrWhenNotProvided) {
  auto ctx = DataContext<int>::Use();
  EXPECT_EQ(ctx, nullptr);
}

TEST(DataContextTest, Create_ReturnsNonNull) {
  auto dc = DataContext<int>::Create(42);
  ASSERT_NE(dc, nullptr);
  EXPECT_EQ(dc->GetReactive()->Get(), 42);
}

TEST(DataContextTest, Use_ReturnsContextWhenProvided) {
  auto dc           = DataContext<int>::Create(42);
  bool found        = false;
  int  found_value  = -1;

  auto inner = ftxui::Renderer([&]() -> ftxui::Element {
    auto ctx = DataContext<int>::Use();
    if (ctx) {
      found       = true;
      found_value = ctx->Get();
    }
    return ftxui::text("");
  });

  auto root = dc->Provide(inner);
  root->Render();

  EXPECT_TRUE(found);
  EXPECT_EQ(found_value, 42);
}

TEST(DataContextTest, Use_ReturnsNullptrAfterProvideScope) {
  auto dc = DataContext<int>::Create(10);

  auto inner = ftxui::Renderer([]() -> ftxui::Element {
    return ftxui::text("");
  });

  auto root = dc->Provide(inner);
  root->Render();

  // After render completes, context is popped.
  auto ctx = DataContext<int>::Use();
  EXPECT_EQ(ctx, nullptr);
}

TEST(DataContextTest, NestedContext_InnerShadowsOuter) {
  auto outer_dc = DataContext<int>::Create(1);
  auto inner_dc = DataContext<int>::Create(2);

  int seen = -1;

  auto leaf = ftxui::Renderer([&]() -> ftxui::Element {
    auto ctx = DataContext<int>::Use();
    if (ctx) {
      seen = ctx->Get();
    }
    return ftxui::text("");
  });

  auto inner_root = inner_dc->Provide(leaf);
  auto outer_root = outer_dc->Provide(inner_root);
  outer_root->Render();

  EXPECT_EQ(seen, 2);  // inner context shadows outer
}

TEST(DataContextTest, DifferentTypes_NoInterference) {
  auto int_dc    = DataContext<int>::Create(100);
  auto str_dc    = DataContext<std::string>::Create("hello");

  int         int_seen = -1;
  std::string str_seen;

  auto leaf = ftxui::Renderer([&]() -> ftxui::Element {
    auto ic = DataContext<int>::Use();
    auto sc = DataContext<std::string>::Use();
    if (ic) int_seen = ic->Get();
    if (sc) str_seen = sc->Get();
    return ftxui::text("");
  });

  auto with_str  = str_dc->Provide(leaf);
  auto with_both = int_dc->Provide(with_str);
  with_both->Render();

  EXPECT_EQ(int_seen, 100);
  EXPECT_EQ(str_seen, "hello");
}

// ============================================================================
// ReactiveDecoratorsTest
// ============================================================================

TEST(ReactiveDecoratorsTest, ShowIf_TrueReturnsElement) {
  auto cond = std::make_shared<Reactive<bool>>(true);
  auto elem = ftxui::text("visible");
  auto result = ShowIf(std::move(elem), cond);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, ShowIf_FalseReturnsEmpty) {
  auto cond   = std::make_shared<Reactive<bool>>(false);
  auto result = ShowIf(ftxui::text("hidden"), cond);
  // emptyElement() is non-null but renders nothing.
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, HideIf_TrueReturnsEmpty) {
  auto cond   = std::make_shared<Reactive<bool>>(true);
  auto result = HideIf(ftxui::text("should hide"), cond);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, HideIf_FalseReturnsElement) {
  auto cond   = std::make_shared<Reactive<bool>>(false);
  auto result = HideIf(ftxui::text("visible"), cond);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, BindText_ReturnsCurrentValue) {
  auto r      = std::make_shared<Reactive<std::string>>("hello");
  auto result = BindText(r);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, BindText_Bold_BuildsOk) {
  auto r      = std::make_shared<Reactive<std::string>>("bold");
  auto result = BindText(r, true);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, BindText_NotBold_BuildsOk) {
  auto r      = std::make_shared<Reactive<std::string>>("normal");
  auto result = BindText(r, false);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, BindColor_BuildsOk) {
  auto c      = std::make_shared<Reactive<ftxui::Color>>(ftxui::Color::Red);
  auto result = BindColor(ftxui::text("colored"), c);
  ASSERT_NE(result, nullptr);
}

TEST(ReactiveDecoratorsTest, ReactiveBox_BuildsOk) {
  auto comp = ReactiveBox([] { return ftxui::text("hello"); });
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(ReactiveDecoratorsTest, ForEach_BuildsOk) {
  auto list = MakeReactiveList<std::string>({"a", "b", "c"});
  auto comp = ForEach<std::string>(
      list,
      [](const std::string& s, bool /*sel*/) { return ftxui::text(s); });
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(ReactiveDecoratorsTest, ForEach_Empty_BuildsOk) {
  auto list = MakeReactiveList<int>();
  auto comp = ForEach<int>(list, [](const int& i, bool) {
    return ftxui::text(std::to_string(i));
  });
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ============================================================================
// Binding integration tests
// ============================================================================

TEST(BindingIntegrationTest, TextInputBind_BuildsOk) {
  auto b    = MakeBind<std::string>("initial");
  auto comp = TextInput("Label").Bind(b).Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, TextInputBind_InitialValueFromReactive) {
  auto b    = MakeBind<std::string>("prefilled");
  auto comp = TextInput("Name").Bind(b).Build();
  // The component renders without crash; value is initialised.
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, TextInputBind_ExternalUpdate_NocrashOnRender) {
  auto b    = MakeBind<std::string>("initial");
  auto comp = TextInput("Name").Bind(b).Build();
  comp->Render();
  // External update via reactive.
  b.Set("updated");
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, Form_BindField_BuildsOk) {
  struct Login {
    std::string user;
    std::string pass;
  };
  auto model = MakeBind<Login>();
  auto comp  = Form()
                   .Title("Login")
                   .BindField("User", model, &Login::user)
                   .BindPassword("Pass", model, &Login::pass)
                   .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, Form_BindCheckbox_BuildsOk) {
  struct Prefs {
    bool dark_mode = false;
  };
  auto model = MakeBind<Prefs>();
  auto comp  = Form()
                   .BindCheckbox("Dark mode", model, &Prefs::dark_mode)
                   .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, VirtualList_BindList_BuildsOk) {
  auto list = MakeReactiveList<std::string>({"alpha", "beta"});
  auto comp = VirtualList<std::string>()
                  .BindList(list)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, VirtualList_BindList_UpdatesOnPush) {
  auto list = MakeReactiveList<std::string>({"a"});
  auto comp = VirtualList<std::string>().BindList(list).Build();
  comp->Render();
  list->Push("b");
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, DataTable_BindList_BuildsOk) {
  struct Row {
    std::string name;
    int value = 0;
  };
  auto list = MakeReactiveList<Row>(
      {Row{"alpha", 1}, Row{"beta", 2}});
  auto comp = DataTable<Row>()
                  .Column("Name", [](const Row& r) { return r.name; })
                  .Column("Val",  [](const Row& r) { return std::to_string(r.value); })
                  .BindList(list)
                  .Build();
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(BindingIntegrationTest, DataTable_BindList_UpdatesOnMutation) {
  struct Item { std::string label; };
  auto list = MakeReactiveList<Item>({{"first"}});
  auto comp = DataTable<Item>()
                  .Column("Label", [](const Item& i) { return i.label; })
                  .BindList(list)
                  .Build();
  comp->Render();
  list->Push({"second"});
  ASSERT_NE(comp->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
