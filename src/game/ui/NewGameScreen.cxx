#include "NewGameScreen.hxx"
#include "UITheme.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "../../simulation/DifficultySettings.hxx"
#include "../../simulation/ScenarioCatalog.hxx"

namespace ui = ImGui;

NewGameScreen::NewGameScreen() {}

void NewGameScreen::draw() const
{
  m_result = e_none;

  constexpr float winW = 840.f;
  constexpr float winH = 570.f;
  const ImVec2 screenSize = ui::GetIO().DisplaySize;

  // Layout constants
  constexpr float SCENARIO_BTN_H = 46.f;  ///< Height of each scenario list button
  constexpr float SCENARIO_BTN_PADDING = 8.f; ///< Horizontal padding inside the list column

  ui::SetNextWindowPos(ImVec2((screenSize.x - winW) * 0.5f, (screenSize.y - winH) * 0.5f));
  ui::SetNextWindowSize(ImVec2(winW, winH));

  UITheme::pushPanelStyle();
  ui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(22.f, 18.f));
  ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 8.f));

  bool open = true;
  ui::BeginCt("NewGameScreen", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  // ── Header ─────────────────────────────────────────────────────────────────
  {
    const char *title = "NEW GAME";
    const float tw = ui::CalcTextSize(title).x;
    ui::SetCursorPosX((winW - tw) * 0.5f);
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
    ui::TextUnformatted(title);
    ui::PopStyleColor();
  }
  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  // ── Two-column layout: scenario list | scenario detail + difficulty ─────────
  const ScenarioCatalog &catalog = ScenarioCatalog::instance();
  const auto &defs = catalog.definitions();

  constexpr float listW = 210.f;
  constexpr float colGap = 18.f;
  constexpr float detailW = winW - listW - colGap - 44.f;
  // Reserve room for header, footer, and padding
  constexpr float sectionH = winH - 130.f;

  // ── Left column: scenario list ─────────────────────────────────────────────
  ui::BeginChild("##ng_list", ImVec2(listW, sectionH), false);
  UITheme::sectionHeader("SCENARIOS");
  ui::Spacing();

  UITheme::pushButtonStyle();
  for (int i = 0; i < static_cast<int>(defs.size()); ++i)
  {
    const bool selected = (m_selectedScenario == i);
    if (selected)
    {
      ui::PushStyleColor(ImGuiCol_Button,        UITheme::COL_BTN_ACTV);
      ui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::COL_BTN_ACTV);
    }
    char btnId[128];
    std::snprintf(btnId, sizeof(btnId), "%s##sc%d", defs[static_cast<size_t>(i)].label.c_str(), i);
    if (ui::ButtonCt(btnId, ImVec2(listW - SCENARIO_BTN_PADDING, SCENARIO_BTN_H)))
      m_selectedScenario = i;
    if (selected)
      ui::PopStyleColor(2);
  }
  UITheme::popButtonStyle();

  ui::EndChild();

  // ── Right column: detail + difficulty ──────────────────────────────────────
  ui::SameLine(0.f, colGap);
  ui::BeginChild("##ng_detail", ImVec2(detailW, sectionH), false);

  if (!defs.empty() && m_selectedScenario < static_cast<int>(defs.size()))
  {
    const ScenarioDefinition &def = defs[static_cast<size_t>(m_selectedScenario)];

    // Scenario name
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
    ui::TextUnformatted(def.label.c_str());
    ui::PopStyleColor();

    ui::Spacing();
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    ui::TextWrapped("%s", def.description.c_str());
    ui::PopStyleColor();

    ui::Spacing();
    ui::Separator();
    ui::Spacing();

    // Starting conditions
    UITheme::sectionHeader("STARTING CONDITIONS");
    ui::Spacing();
    ui::Text("Approval:  %.0f / 100", def.startingApproval);
    const ImVec4 balCol = def.startingBalance >= 0.f ? UITheme::COL_GREEN : UITheme::COL_RED;
    ui::TextColored(balCol, "Balance:   %+.0f", def.startingBalance);

    if (!def.recommendedPolicies.empty())
    {
      ui::Spacing();
      UITheme::sectionHeader("RECOMMENDED POLICIES");
      ui::Spacing();
      for (const auto &policyId : def.recommendedPolicies)
      {
        ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
        ui::BulletText("%s", policyId.c_str());
        ui::PopStyleColor();
      }
    }
  }

  ui::Spacing();
  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  // ── Difficulty selector ────────────────────────────────────────────────────
  UITheme::sectionHeader("DIFFICULTY");
  ui::Spacing();

  constexpr DifficultyLevel kLevels[3] = {
      DifficultyLevel::Sandbox,
      DifficultyLevel::Standard,
      DifficultyLevel::Challenge};
  const float diffBtnW = (detailW - 8.f) / 3.f;
  constexpr float diffBtnH = 38.f;

  UITheme::pushButtonStyle();
  for (int i = 0; i < 3; ++i)
  {
    if (i > 0)
      ui::SameLine(0.f, 4.f);

    const bool active = (m_selectedDifficulty == kLevels[i]);
    if (active)
    {
      ui::PushStyleColor(ImGuiCol_Button,        UITheme::COL_BTN_ACTV);
      ui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::COL_BTN_ACTV);
    }
    char diffId[64];
    std::snprintf(diffId, sizeof(diffId), "%s##diff%d", difficultyLabel(kLevels[i]), i);
    if (ui::ButtonCt(diffId, ImVec2(diffBtnW, diffBtnH)))
      m_selectedDifficulty = kLevels[i];
    if (active)
      ui::PopStyleColor(2);
    if (ui::IsItemHovered())
      ui::SetTooltip("%s", difficultyDescription(kLevels[i]));
  }
  UITheme::popButtonStyle();

  // Difficulty description
  ui::Spacing();
  ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
  ui::TextWrapped("%s", difficultyDescription(m_selectedDifficulty));
  ui::PopStyleColor();

  ui::EndChild();

  // ── Footer ─────────────────────────────────────────────────────────────────
  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  constexpr float footerBtnW = 150.f;
  constexpr float footerBtnH = 38.f;

  UITheme::pushButtonStyle();

  // Back button (left)
  if (ui::ButtonCt("Back", ImVec2(footerBtnW, footerBtnH)))
    m_result = e_close;

  // Start Game button (right, green tint)
  ui::SameLine(winW - 44.f - footerBtnW);
  ui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.40f, 0.10f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.58f, 0.18f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.07f, 0.32f, 0.07f, 1.f));
  const bool hasScenarios = !defs.empty();
  if (!hasScenarios)
    ui::BeginDisabled();
  if (ui::ButtonCt("Start Game", ImVec2(footerBtnW, footerBtnH)))
  {
    if (hasScenarios)
    {
      ScenarioCatalog::instance().setPendingSelection(
          defs[static_cast<size_t>(m_selectedScenario)].id, m_selectedDifficulty);
    }
    m_result = e_start_game;
  }
  if (!hasScenarios)
  {
    ui::EndDisabled();
    if (ui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
      ui::SetTooltip("No scenarios found. Cannot start a new game.");
  }
  ui::PopStyleColor(3);

  UITheme::popButtonStyle();

  ui::PopStyleVar(2);
  UITheme::popPanelStyle();

  ui::End();
}
