// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/router.hpp"

#include "ftxui/component/component.hpp"
#include "ftxui/dom/elements.hpp"
#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// Minimal stub component for route targets.
ftxui::Component MakeStub(const std::string& name) {
  return ftxui::Renderer([name] { return ftxui::text(name); });
}

// ── Build / Default route
// ──────────────────────────────────────────────────────

TEST(RouterTest, CurrentReturnsDefaultRouteAfterBuild) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("other", MakeStub("other"))
      .Default("home");

  auto root = router.Build();
  EXPECT_EQ(router.Current(), "home");
}

TEST(RouterTest, BuildReturnsNonNullComponent) {
  Router router;
  router.Route("home", MakeStub("home")).Default("home");
  auto root = router.Build();
  ASSERT_NE(root, nullptr);
}

TEST(RouterTest, RenderWithNoRoutesDoesNotCrash) {
  Router router;
  auto root = router.Build();
  // Should render "(no route)" without crashing.
  ASSERT_NE(root, nullptr);
  // Render without a screen — just verify the element is built.
  auto elem = root->Render();
  ASSERT_NE(elem, nullptr);
}

// ── Push
// ───────────────────────────────────────────────────────────────────────

TEST(RouterTest, PushChangesCurrentRoute) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("settings", MakeStub("settings"))
      .Default("home");
  router.Build();

  EXPECT_EQ(router.Current(), "home");
  router.Push("settings");
  EXPECT_EQ(router.Current(), "settings");
}

TEST(RouterTest, PushUnknownRouteIsNoOp) {
  Router router;
  router.Route("home", MakeStub("home")).Default("home");
  router.Build();

  EXPECT_EQ(router.Current(), "home");
  // Pushing an unknown route should not crash and should leave current
  // unchanged.
  router.Push("nonexistent");
  EXPECT_EQ(router.Current(), "home");
}

// ── Pop
// ────────────────────────────────────────────────────────────────────────

TEST(RouterTest, PopReturnsToPreviousRoute) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("settings", MakeStub("settings"))
      .Default("home");
  router.Build();

  router.Push("settings");
  EXPECT_EQ(router.Current(), "settings");

  router.Pop();
  EXPECT_EQ(router.Current(), "home");
}

TEST(RouterTest, PopOnRootIsNoOp) {
  Router router;
  router.Route("home", MakeStub("home")).Default("home");
  router.Build();

  EXPECT_EQ(router.Current(), "home");
  EXPECT_FALSE(router.CanGoBack());

  // Pop on a single-entry stack should be a no-op.
  router.Pop();
  EXPECT_EQ(router.Current(), "home");
}

TEST(RouterTest, CanGoBackFalseOnRoot) {
  Router router;
  router.Route("home", MakeStub("home")).Default("home");
  router.Build();

  EXPECT_FALSE(router.CanGoBack());
}

TEST(RouterTest, CanGoBackTrueAfterPush) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("detail", MakeStub("detail"))
      .Default("home");
  router.Build();

  EXPECT_FALSE(router.CanGoBack());
  router.Push("detail");
  EXPECT_TRUE(router.CanGoBack());
  router.Pop();
  EXPECT_FALSE(router.CanGoBack());
}

// ── Replace
// ────────────────────────────────────────────────────────────────────

TEST(RouterTest, ReplaceChangesCurrentWithoutAddingHistory) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("a", MakeStub("a"))
      .Route("b", MakeStub("b"))
      .Default("home");
  router.Build();

  router.Push("a");
  EXPECT_EQ(router.Current(), "a");
  EXPECT_TRUE(router.CanGoBack());

  router.Replace("b");
  EXPECT_EQ(router.Current(), "b");

  // Pop should go back to "home", not "a" — Replace didn't add "a" twice.
  router.Pop();
  EXPECT_EQ(router.Current(), "home");
}

TEST(RouterTest, ReplaceUnknownRouteIsNoOp) {
  Router router;
  router.Route("home", MakeStub("home")).Default("home");
  router.Build();

  EXPECT_EQ(router.Current(), "home");
  router.Replace("nonexistent");
  EXPECT_EQ(router.Current(), "home");
}

// ── Lazy factory routes
// ────────────────────────────────────────────────────────

TEST(RouterTest, LazyFactoryRouteCreatesComponentOnFirstRender) {
  int create_count = 0;
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("lazy",
             [&create_count]() -> ftxui::Component {
               create_count++;
               return MakeStub("lazy");
             })
      .Default("home");
  auto root = router.Build();

  EXPECT_EQ(create_count, 0);
  router.Push("lazy");
  EXPECT_EQ(router.Current(), "lazy");

  // Factory is called on first render, not on navigation.
  EXPECT_EQ(create_count, 0);
  root->Render();  // Triggers lazy creation.
  EXPECT_EQ(create_count, 1);

  // Navigating away and back should reuse the cached instance.
  router.Pop();
  router.Push("lazy");
  root->Render();
  EXPECT_EQ(create_count, 1);
}

// ── OnNavigate callback
// ────────────────────────────────────────────────────────

TEST(RouterTest, OnNavigateCallbackFires) {
  std::string last_from;
  std::string last_to;

  Router router;
  router.Route("home", MakeStub("home"))
      .Route("other", MakeStub("other"))
      .Default("home")
      .OnNavigate([&](std::string_view from, std::string_view to) {
        last_from = from;
        last_to = to;
      });
  router.Build();

  router.Push("other");
  EXPECT_EQ(last_to, "other");

  router.Pop();
  EXPECT_EQ(last_to, "home");
}

// ── Multiple Build calls share state
// ──────────────────────────────────────────

TEST(RouterTest, MultipleBuildCallsShareNavState) {
  Router router;
  router.Route("home", MakeStub("home"))
      .Route("page2", MakeStub("page2"))
      .Default("home");

  auto root1 = router.Build();
  auto root2 = router.Build();

  router.Push("page2");
  EXPECT_EQ(router.Current(), "page2");

  // Both components should render the same active route without crashing.
  ASSERT_NE(root1->Render(), nullptr);
  ASSERT_NE(root2->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
