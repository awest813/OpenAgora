#include "PolicyPanel.hxx"
#include "UITheme.hxx"

#include <PolicyEngine.hxx>
#include <BudgetSystem.hxx>

#include "imgui.h"

#include <cstdio>

namespace ui = ImGui;

// ── draw() ────────────────────────────────────────────────────────────────────

void PolicyPanel::draw() const
{
  const auto &definitions = PolicyEngine::instance().definitions();
  const int   policyCount = static_cast<int>(definitions.size());

  // Dynamically size the panel to fit all policies (min 1 row).
  constexpr float panelWidth   = 260.f;
  constexpr float headerHeight = 80.f;  // title + budget summary
  constexpr float rowHeight    = 38.f;  // per-policy row
  const float     panelHeight  = headerHeight + std::max(1, policyCount) * rowHeight + 12.f;

  // Sits to the right of the left-side panels (which end at ~480 in y).
  const ImVec2 panelPos{6.f, 480.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##policy_panel", &open, flags))
  {
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  UITheme::sectionHeader("  Policies");

  // ── Budget summary ─────────────────────────────────────────────────────────
  {
    const BudgetSystem &budget = BudgetSystem::instance();
    const float balance = budget.currentBalance();

    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    ui::Text("Balance: ");
    ui::SameLine();
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, balance >= 0.f ? UITheme::COL_GREEN : UITheme::COL_RED);
    ui::Text("%.0f", balance);
    ImGui::PopStyleColor();

    ui::SameLine(0.f, 8.f);
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    ui::Text("(+%.0f / -%.0f)", budget.lastTaxRevenue(), budget.lastExpenses());
    ImGui::PopStyleColor();
  }

  ui::Spacing();

  // ── Policy rows ────────────────────────────────────────────────────────────
  if (definitions.empty())
  {
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    ui::TextUnformatted("  No policies loaded.");
    ImGui::PopStyleColor();
  }
  else
  {
    PolicyEngine &policyEngine = PolicyEngine::instance();

    UITheme::pushButtonStyle();
    for (const auto &def : definitions)
    {
      bool active = policyEngine.isActive(def.id);

      // Checkbox label is the policy name
      if (ui::Checkbox(def.label.c_str(), &active))
        policyEngine.setActive(def.id, active);

      if (ui::IsItemHovered())
        ui::SetTooltip("%s", def.description.c_str());

      // Show monthly cost on the same line, right-aligned
      if (def.costPerMonth > 0)
      {
        ui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, active ? UITheme::COL_YELLOW : UITheme::COL_TEXT_DISABLED);
        ui::Text("-%d/mo", def.costPerMonth);
        ImGui::PopStyleColor();
      }
    }
    UITheme::popButtonStyle();
  }

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
