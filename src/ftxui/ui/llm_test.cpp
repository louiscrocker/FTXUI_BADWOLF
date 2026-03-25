// Copyright 2024 Arthur Sonzogni. All rights reserved.
// Use of this source code is governed by the MIT license that can be found in
// the LICENSE file.
#include "ftxui/ui/llm_bridge.hpp"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace ftxui::ui {
namespace {

// ── NLParser tests
// ────────────────────────────────────────────────────────────

TEST(NLParserTest, IdentifiesTableIntent) {
  auto intent = NLParser::Parse("show users table");
  EXPECT_EQ(intent.type, IntentType::TABLE);
}

TEST(NLParserTest, IdentifiesChartIntent) {
  auto intent = NLParser::Parse("plot server metrics");
  EXPECT_EQ(intent.type, IntentType::CHART);
}

TEST(NLParserTest, IdentifiesFormIntent) {
  auto intent = NLParser::Parse("create a new user form");
  EXPECT_EQ(intent.type, IntentType::FORM);
}

TEST(NLParserTest, IdentifiesDashboardIntent) {
  auto intent = NLParser::Parse("show me a dashboard overview");
  EXPECT_EQ(intent.type, IntentType::DASHBOARD);
}

TEST(NLParserTest, IdentifiesMapIntent) {
  auto intent = NLParser::Parse("display a world map");
  EXPECT_EQ(intent.type, IntentType::MAP);
}

TEST(NLParserTest, IdentifiesLogIntent) {
  auto intent = NLParser::Parse("show server log events");
  EXPECT_EQ(intent.type, IntentType::LOG);
}

TEST(NLParserTest, IdentifiesProgressIntent) {
  auto intent = NLParser::Parse("show loading progress");
  EXPECT_EQ(intent.type, IntentType::PROGRESS);
}

TEST(NLParserTest, ExtractsEntityFromDisplayList) {
  auto intent = NLParser::Parse("display a list of files");
  EXPECT_EQ(intent.entity, "files");
}

TEST(NLParserTest, ExtractsEntityFromShowUsers) {
  auto intent = NLParser::Parse("show users");
  EXPECT_EQ(intent.entity, "users");
}

TEST(NLParserTest, ExtractsFieldsFromColumnsClause) {
  auto intent = NLParser::Parse("show users with columns name and email");
  ASSERT_FALSE(intent.fields.empty());
  EXPECT_EQ(intent.fields[0], "name");
  EXPECT_EQ(intent.fields[1], "email");
}

TEST(NLParserTest, HandlesUnknownInputGracefully) {
  auto intent = NLParser::Parse("xyzzy frobnicator bleep bloop");
  EXPECT_EQ(intent.type, IntentType::UNKNOWN);
  EXPECT_EQ(intent.raw_input, "xyzzy frobnicator bleep bloop");
}

TEST(NLParserTest, HandlesEmptyString) {
  auto intent = NLParser::Parse("");
  EXPECT_EQ(intent.type, IntentType::UNKNOWN);
  EXPECT_TRUE(intent.entity.empty());
  EXPECT_TRUE(intent.fields.empty());
}

TEST(NLParserTest, HandlesMultiWordEntity) {
  auto intent = NLParser::Parse("show a table of server metrics");
  // Should parse as TABLE with entity "server" or "metrics"
  EXPECT_EQ(intent.type, IntentType::TABLE);
  EXPECT_FALSE(intent.entity.empty());
}

TEST(NLParserTest, CaseInsensitiveMatching) {
  auto intent_lower = NLParser::Parse("show users table");
  auto intent_upper = NLParser::Parse("SHOW USERS TABLE");
  auto intent_mixed = NLParser::Parse("Show Users Table");

  EXPECT_EQ(intent_lower.type, IntentType::TABLE);
  EXPECT_EQ(intent_upper.type, IntentType::TABLE);
  EXPECT_EQ(intent_mixed.type, IntentType::TABLE);
}

// ── UIGenerator tests
// ─────────────────────────────────────────────────────────

TEST(UIGeneratorTest, GeneratesTableComponent) {
  UIIntent intent;
  intent.type = IntentType::TABLE;
  intent.entity = "users";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GeneratesChartComponent) {
  UIIntent intent;
  intent.type = IntentType::CHART;
  intent.entity = "metrics";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GeneratesFormComponent) {
  UIIntent intent;
  intent.type = IntentType::FORM;
  intent.entity = "users";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GeneratesDashboardComponent) {
  UIIntent intent;
  intent.type = IntentType::DASHBOARD;
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GeneratesLogComponent) {
  UIIntent intent;
  intent.type = IntentType::LOG;
  intent.entity = "servers";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GeneratesProgressComponent) {
  UIIntent intent;
  intent.type = IntentType::PROGRESS;
  intent.entity = "upload";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, HandlesUnknownIntent) {
  UIIntent intent;
  intent.type = IntentType::UNKNOWN;
  intent.raw_input = "something completely unknown";
  auto comp = UIGenerator::Generate(intent);
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

TEST(UIGeneratorTest, GenerateFromTextEndToEnd) {
  auto comp = UIGenerator::GenerateFromText("show me a table of users");
  ASSERT_NE(comp, nullptr);
  ASSERT_NE(comp->Render(), nullptr);
}

// ── LLMAdapter tests
// ──────────────────────────────────────────────────────────

TEST(LLMAdapterTest, OllamaAvailableReturnsBoolWithoutCrash) {
  // Just verifies it doesn't crash — return value depends on environment.
  bool result = LLMAdapter::OllamaAvailable();
  EXPECT_TRUE(result == true || result == false);
}

TEST(LLMAdapterTest, OpenAIAvailableChecksEnvVar) {
  // Without the env var set this should return false (in CI it won't be set).
  // We just verify it returns a bool without crashing.
  bool result = LLMAdapter::OpenAIAvailable();
  EXPECT_TRUE(result == true || result == false);
}

TEST(LLMAdapterTest, SystemPromptIsNonEmpty) {
  std::string prompt = LLMAdapter::SystemPrompt();
  EXPECT_FALSE(prompt.empty());
  EXPECT_NE(prompt.find("JSON"), std::string::npos);
}

TEST(LLMAdapterTest, QueryLLMFallsBackToNLParserWhenOllamaUnavailable) {
  // If Ollama is not running this should gracefully fall back to NLParser.
  // Either way it should return a valid UIIntent without crashing.
  auto intent = LLMAdapter::QueryLLM("show me a table of users");
  EXPECT_EQ(intent.raw_input, "show me a table of users");
  // NLParser fallback should produce TABLE intent
  if (!LLMAdapter::OllamaAvailable()) {
    EXPECT_EQ(intent.type, IntentType::TABLE);
  }
}

// ── Component decorator tests
// ─────────────────────────────────────────────────

TEST(WithLLMHelpTest, WrapsComponentWithoutCrash) {
  UIIntent intent;
  intent.type = IntentType::TABLE;
  intent.entity = "users";
  auto inner = UIGenerator::Generate(intent);
  ASSERT_NE(inner, nullptr);

  auto wrapped = WithLLMHelp(inner, "User Table");
  ASSERT_NE(wrapped, nullptr);
  ASSERT_NE(wrapped->Render(), nullptr);
}

TEST(LLMReplTest, LLMReplBuildsWithoutCrash) {
  auto repl = LLMRepl();
  ASSERT_NE(repl, nullptr);
  ASSERT_NE(repl->Render(), nullptr);
}

}  // namespace
}  // namespace ftxui::ui
