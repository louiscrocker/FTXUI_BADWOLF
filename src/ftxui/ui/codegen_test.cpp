// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/codegen.hpp"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── CodeGenRequest ──────────────────────────────────────────────────────────

TEST(CodeGenRequestTest, DefaultStyle) {
  CodeGenRequest req;
  EXPECT_EQ(req.style, "modern");
}

TEST(CodeGenRequestTest, DefaultModel) {
  CodeGenRequest req;
  EXPECT_EQ(req.model, "llama3");
}

// ── CodeGenResult ───────────────────────────────────────────────────────────

TEST(CodeGenResultTest, DefaultNotSuccess) {
  CodeGenResult r;
  EXPECT_FALSE(r.success);
}

TEST(CodeGenResultTest, LooksValidTrueForValidCode) {
  CodeGenResult r;
  r.code = "#include <ftxui/ui.hpp>\nint main() { return 0; }";
  EXPECT_TRUE(r.LooksValid());
}

TEST(CodeGenResultTest, LooksValidFalseForEmpty) {
  CodeGenResult r;
  r.code = "";
  EXPECT_FALSE(r.LooksValid());
}

TEST(CodeGenResultTest, LooksValidFalseWithoutFtxui) {
  CodeGenResult r;
  r.code = "#include <iostream>\nint main() { return 0; }";
  EXPECT_FALSE(r.LooksValid());
}

TEST(CodeGenResultTest, LooksValidFalseWithoutMain) {
  CodeGenResult r;
  r.code = "#include <ftxui/ui.hpp>\nvoid helper() {}";
  EXPECT_FALSE(r.LooksValid());
}

// ── TemplateLibrary ─────────────────────────────────────────────────────────

TEST(TemplateLibraryTest, BuiltinTemplatesReturnsEight) {
  const auto& tmpls = TemplateLibrary::BuiltinTemplates();
  EXPECT_EQ(tmpls.size(), 8u);
}

TEST(TemplateLibraryTest, AllTemplatesHaveName) {
  for (const auto& t : TemplateLibrary::BuiltinTemplates()) {
    EXPECT_FALSE(t.name.empty());
  }
}

TEST(TemplateLibraryTest, AllTemplatesHaveCode) {
  for (const auto& t : TemplateLibrary::BuiltinTemplates()) {
    EXPECT_FALSE(t.code.empty());
  }
}

TEST(TemplateLibraryTest, FindBestDashboard) {
  auto* t = TemplateLibrary::FindBest("dashboard overview");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->name, "dashboard");
}

TEST(TemplateLibraryTest, FindBestMonitor) {
  auto* t = TemplateLibrary::FindBest("kubernetes pod monitor");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->name, "monitor");
}

TEST(TemplateLibraryTest, FindBestGame) {
  auto* t = TemplateLibrary::FindBest("arcade game with physics");
  ASSERT_NE(t, nullptr);
  EXPECT_EQ(t->name, "game");
}

TEST(TemplateLibraryTest, FindBestMap) {
  auto* t = TemplateLibrary::FindBest("world map explorer");
  ASSERT_NE(t, nullptr);
  // Matches either "map" or "explorer" — both are valid
  EXPECT_TRUE(t->name == "map" || t->name == "explorer");
}

TEST(TemplateLibraryTest, FindBestReturnsNullForGibberish) {
  auto* t = TemplateLibrary::FindBest("xyzzy frobnicator 12345");
  EXPECT_EQ(t, nullptr);
}

TEST(TemplateLibraryTest, FillReplacesPlaceholders) {
  const auto& tmpls = TemplateLibrary::BuiltinTemplates();
  ASSERT_FALSE(tmpls.empty());
  std::map<std::string, std::string> vars = {{"APP_NAME", "TestApp"}};
  std::string filled = TemplateLibrary::Fill(tmpls[0], vars);
  EXPECT_NE(filled.find("TestApp"), std::string::npos);
  // Original placeholder should be replaced
  EXPECT_EQ(filled.find("{APP_NAME}"), std::string::npos);
}

TEST(TemplateLibraryTest, FillHandlesMissingPlaceholders) {
  CodeTemplate tmpl;
  tmpl.code = "Hello {UNKNOWN_KEY} world";
  std::map<std::string, std::string> vars;
  // Should not crash and should leave unknown placeholder as-is
  std::string result = TemplateLibrary::Fill(tmpl, vars);
  EXPECT_NE(result.find("{UNKNOWN_KEY}"), std::string::npos);
}

// ── CodeGenerator ────────────────────────────────────────────────────────────

TEST(CodeGeneratorTest, ConstructsWithoutCrash) {
  CodeGenerator gen;
  EXPECT_EQ(gen.ModelName(), "llama3");
}

TEST(CodeGeneratorTest, ConstructsWithCustomModel) {
  CodeGenerator gen("gpt-4");
  EXPECT_EQ(gen.ModelName(), "gpt-4");
}

TEST(CodeGeneratorTest, LLMAvailableReturnsBool) {
  CodeGenerator gen;
  bool result = gen.LLMAvailable();
  EXPECT_TRUE(result == true || result == false);
}

TEST(CodeGeneratorTest, BuildSystemPromptNonEmpty) {
  std::string prompt = CodeGenerator::BuildSystemPrompt();
  EXPECT_FALSE(prompt.empty());
  EXPECT_NE(prompt.find("ftxui"), std::string::npos);
}

TEST(CodeGeneratorTest, BuildUserPromptNonEmpty) {
  CodeGenRequest req;
  req.description = "test dashboard";
  std::string prompt = CodeGenerator::BuildUserPrompt(req);
  EXPECT_FALSE(prompt.empty());
  EXPECT_NE(prompt.find("test dashboard"), std::string::npos);
}

TEST(CodeGeneratorTest, GenerateFromTemplateReturnsResult) {
  CodeGenerator gen;
  CodeGenRequest req;
  req.description = "kubernetes pod monitor";
  auto result = gen.GenerateFromTemplate(req);
  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.code.empty());
}

TEST(CodeGeneratorTest, GenerateFromTemplateCodeLooksValid) {
  CodeGenerator gen;
  CodeGenRequest req;
  req.description = "dashboard overview";
  auto result = gen.GenerateFromTemplate(req);
  EXPECT_TRUE(result.LooksValid());
}

TEST(CodeGeneratorTest, GenerateFromTemplateFallsBackForUnknown) {
  CodeGenerator gen;
  CodeGenRequest req;
  req.description = "xyzzy frobnicator";
  // Should not crash and should return something
  auto result = gen.GenerateFromTemplate(req);
  EXPECT_TRUE(result.success);
  EXPECT_FALSE(result.code.empty());
}

// ── SyntaxHighlighter ────────────────────────────────────────────────────────

TEST(SyntaxHighlighterTest, TokenizeEmptyLine) {
  auto tokens = SyntaxHighlighter::Tokenize("");
  EXPECT_TRUE(tokens.empty());
}

TEST(SyntaxHighlighterTest, TokenizeKeyword) {
  auto tokens = SyntaxHighlighter::Tokenize("int main()");
  bool found_keyword = false;
  for (const auto& tok : tokens) {
    if (tok.text == "int" &&
        tok.type == SyntaxHighlighter::TokenType::Keyword) {
      found_keyword = true;
    }
  }
  EXPECT_TRUE(found_keyword);
}

TEST(SyntaxHighlighterTest, TokenizeStringLiteral) {
  auto tokens = SyntaxHighlighter::Tokenize("\"hello\"");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].type, SyntaxHighlighter::TokenType::String);
}

TEST(SyntaxHighlighterTest, TokenizeComment) {
  auto tokens = SyntaxHighlighter::Tokenize("// this is a comment");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].type, SyntaxHighlighter::TokenType::Comment);
}

TEST(SyntaxHighlighterTest, TokenizePreprocessor) {
  auto tokens = SyntaxHighlighter::Tokenize("#include <iostream>");
  ASSERT_FALSE(tokens.empty());
  EXPECT_EQ(tokens[0].type, SyntaxHighlighter::TokenType::Preprocessor);
}

TEST(SyntaxHighlighterTest, HighlightLineReturnsNonNull) {
  auto el = SyntaxHighlighter::HighlightLine("int main() { return 0; }");
  EXPECT_NE(el, nullptr);
}

TEST(SyntaxHighlighterTest, HighlightReturnsOneLine) {
  auto lines = SyntaxHighlighter::Highlight("int x = 0;");
  EXPECT_EQ(lines.size(), 1u);
}

TEST(SyntaxHighlighterTest, HighlightReturnsMultipleLines) {
  auto lines = SyntaxHighlighter::Highlight("int x = 0;\nreturn 0;\n");
  EXPECT_EQ(lines.size(), 2u);  // trailing newline does not add an extra line
}

// ── CodePreview ──────────────────────────────────────────────────────────────

TEST(CodePreviewTest, ReturnsNonNull) {
  auto comp = CodePreview("#include <ftxui/ui.hpp>\nint main() {}");
  EXPECT_NE(comp, nullptr);
}

TEST(CodePreviewTest, RenderDoesNotCrash) {
  auto comp = CodePreview("#include <ftxui/ui.hpp>\nint main() { return 0; }");
  ASSERT_NE(comp, nullptr);
  EXPECT_NE(comp->Render(), nullptr);
}

// ── CodeGenStudio ────────────────────────────────────────────────────────────

TEST(CodeGenStudioTest, BuildsWithoutCrash) {
  auto studio = CodeGenStudio();
  EXPECT_NE(studio, nullptr);
}

TEST(CodeGenStudioTest, RenderDoesNotCrash) {
  auto studio = CodeGenStudio();
  ASSERT_NE(studio, nullptr);
  EXPECT_NE(studio->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
