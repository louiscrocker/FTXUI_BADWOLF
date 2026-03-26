// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/plugin.hpp"

#include <string>
#include <vector>

#include "ftxui/component/component.hpp"
#include "gtest/gtest.h"

// ── 14 (compile-time): BADWOLF_PLUGIN_DEFINE macro ───────────────────────────
// Placed at file scope to ensure extern "C" functions have proper global
// linkage and _badwolf_plugin_info_ is accessible from the test below.
BADWOLF_PLUGIN_DEFINE("compile-check",
                      "1.0.0",
                      "Compile test plugin",
                      "Dev",
                      (std::vector<std::string>{"CompileWidget"}))

namespace ftxui::ui {
namespace {

// ── Helper
// ────────────────────────────────────────────────────────────────────

ftxui::Component MakeWidget() {
  return ftxui::Renderer([] { return ftxui::text("test-widget"); });
}

// ── 1. PluginInfo construction
// ────────────────────────────────────────────────

TEST(PluginSystemTest, PluginInfoConstruction) {
  PluginInfo info{
      "myplugin", "2.0.0", "My Plugin", "Alice", {"WidgetA", "WidgetB"}};
  EXPECT_EQ(info.name, "myplugin");
  EXPECT_EQ(info.version, "2.0.0");
  EXPECT_EQ(info.description, "My Plugin");
  EXPECT_EQ(info.author, "Alice");
  ASSERT_EQ(info.provides.size(), 2u);
  EXPECT_EQ(info.provides[0], "WidgetA");
  EXPECT_EQ(info.provides[1], "WidgetB");
}

// ── 2. PluginRegistry::Instance() returns singleton ──────────────────────────

TEST(PluginSystemTest, RegistrySingleton) {
  PluginRegistry& a = PluginRegistry::Instance();
  PluginRegistry& b = PluginRegistry::Instance();
  EXPECT_EQ(&a, &b);
}

// ── 3. PluginRegistry initial plugins list is empty ──────────────────────────

TEST(PluginSystemTest, RegistryInitialPluginsEmpty) {
  // Relies on running before any plugin registration tests.
  // All subsequent registration tests clean up after themselves.
  PluginRegistry& reg = PluginRegistry::Instance();
  EXPECT_TRUE(reg.Plugins().empty());
}

// ── 4. PluginRegistry SearchPaths initially empty ────────────────────────────

TEST(PluginSystemTest, RegistrySearchPathsInitiallyEmpty) {
  PluginRegistry& reg = PluginRegistry::Instance();
  EXPECT_TRUE(reg.SearchPaths().empty());
}

// ── 5. PluginRegistry AddSearchPath adds path ────────────────────────────────

TEST(PluginSystemTest, RegistryAddSearchPath) {
  PluginRegistry& reg = PluginRegistry::Instance();
  const size_t before = reg.SearchPaths().size();

  reg.AddSearchPath("/tmp/plugins");
  EXPECT_EQ(reg.SearchPaths().size(), before + 1u);
  EXPECT_EQ(reg.SearchPaths().back(), "/tmp/plugins");
}

// ── 6. PluginRegistry Load with invalid path returns nullptr ─────────────────

TEST(PluginSystemTest, RegistryLoadInvalidPathReturnsNullptr) {
  PluginRegistry& reg = PluginRegistry::Instance();
  auto handle = reg.Load("/nonexistent/path/to/plugin.so");
  EXPECT_EQ(handle, nullptr);
}

// ── 7. PluginRegistry last_error set on load failure ─────────────────────────

TEST(PluginSystemTest, RegistryLastErrorSetOnLoadFailure) {
  PluginRegistry& reg = PluginRegistry::Instance();
  reg.Load("/nonexistent/path/to/plugin.dylib");
  EXPECT_FALSE(reg.last_error().empty());
}

// ── 8. PluginRegistry FindProvider returns nullptr for unknown component
// ──────

TEST(PluginSystemTest, RegistryFindProviderUnknownReturnsNullptr) {
  PluginRegistry& reg = PluginRegistry::Instance();
  auto handle = reg.FindProvider("DoesNotExist_XYZ_9182736");
  EXPECT_EQ(handle, nullptr);
}

// ── 9. InProcessPlugin construction ──────────────────────────────────────────

TEST(PluginSystemTest, InProcessPluginConstruction) {
  InProcessPlugin plugin(
      PluginInfo{"ip-construct-test", "1.0.0", "Test", "Dev", {"TestWidget"}},
      [](const std::string& /*name*/, const std::string& /*params*/) {
        return MakeWidget();
      });
  // Construction should not throw or crash.
  SUCCEED();
}

// ── 10. InProcessPlugin Register + FindProvider works ────────────────────────

TEST(PluginSystemTest, InProcessPluginRegisterAndFindProvider) {
  InProcessPlugin plugin(
      PluginInfo{"ip-reg-test", "1.0.0", "Test", "Dev", {"IPWidget"}},
      [](const std::string& /*name*/, const std::string& /*params*/) {
        return MakeWidget();
      });

  plugin.Register();

  auto handle = PluginRegistry::Instance().FindProvider("IPWidget");
  EXPECT_NE(handle, nullptr);
  EXPECT_EQ(handle->Info().name, "ip-reg-test");

  plugin.Unregister();  // clean up singleton
}

// ── 11. InProcessPlugin Create returns non-null component ────────────────────

TEST(PluginSystemTest, InProcessPluginCreateReturnsComponent) {
  InProcessPlugin plugin(
      PluginInfo{"ip-create-test", "1.0.0", "Test", "Dev", {"CreatableWidget"}},
      [](const std::string& name, const std::string& /*params*/) {
        if (name == "CreatableWidget") {
          return MakeWidget();
        }
        return ftxui::Component{nullptr};
      });

  plugin.Register();

  auto comp = PluginRegistry::Instance().Create("CreatableWidget");
  EXPECT_NE(comp, nullptr);

  plugin.Unregister();  // clean up singleton
}

// ── 12. InProcessPlugin Unregister removes from registry ─────────────────────

TEST(PluginSystemTest, InProcessPluginUnregisterRemovesFromRegistry) {
  InProcessPlugin plugin(
      PluginInfo{"ip-unreg-test", "1.0.0", "Test", "Dev", {"UnregWidget"}},
      [](const std::string& /*name*/, const std::string& /*params*/) {
        return MakeWidget();
      });

  plugin.Register();
  EXPECT_NE(PluginRegistry::Instance().FindProvider("UnregWidget"), nullptr);

  plugin.Unregister();
  EXPECT_EQ(PluginRegistry::Instance().FindProvider("UnregWidget"), nullptr);
}

// ── 13. PluginManager component builds without crash ─────────────────────────

TEST(PluginSystemTest, PluginManagerBuildsWithoutCrash) {
  ftxui::Component manager = PluginManager();
  EXPECT_NE(manager, nullptr);
  // Render once to ensure no crash.
  auto element = manager->Render();
  EXPECT_NE(element, nullptr);
}

// ── 14. BADWOLF_PLUGIN_DEFINE macro compiles (file-scope expansion above)
// ─────

TEST(PluginSystemTest, BadwolfPluginDefineMacroCompiles) {
  // The macro was expanded at file scope above. Verify the output is correct.
  const ftxui::ui::PluginInfo& info = ::_badwolf_plugin_info_;
  EXPECT_EQ(info.name, "compile-check");
  EXPECT_EQ(info.version, "1.0.0");
  ASSERT_EQ(info.provides.size(), 1u);
  EXPECT_EQ(info.provides[0], "CompileWidget");
  EXPECT_STREQ(::badwolf_plugin_version(), "1.0.0");
}

// ── 15. PluginRegistry LoadDirectory on nonexistent dir returns 0
// ─────────────

TEST(PluginSystemTest, RegistryLoadDirectoryNonexistentReturnsZero) {
  PluginRegistry& reg = PluginRegistry::Instance();
  int count = reg.LoadDirectory("/nonexistent/plugin/directory/xyz");
  EXPECT_EQ(count, 0);
}

}  // namespace
}  // namespace ftxui::ui
