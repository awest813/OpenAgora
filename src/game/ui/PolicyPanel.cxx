#include "PolicyPanel.hxx"
#include "UITheme.hxx"

#include <PolicyEngine.hxx>
#include <BudgetSystem.hxx>
#include <GovernanceSystem.hxx>

#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace ui = ImGui;

namespace
{
int costForLevel(const PolicyDefinition &definition, int level)
{
  const auto it = std::find_if(definition.levels.begin(), definition.levels.end(),
                               [level](const PolicyLevelDefinition &entry) { return entry.level == level; });
  if (it == definition.levels.end())
    return definition.costPerMonth;
  return it->costPerMonth;
}
} // namespace

// ── draw() ────────────────────────────────────────────────────────────────────

void PolicyPanel::draw() const
{
  const auto &definitions = PolicyEngine::instance().definitions();
  const int   policyCount = static_cast<int>(definitions.size());

  constexpr float panelWidth   = 340.f;
  constexpr float panelHeight  = 360.f;

  // Sits below the Governance Panel (which now ends at ~494 with new pledge/win content).
  const ImVec2 panelPos{6.f, 500.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

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
    GovernanceSystem &governance = GovernanceSystem::instance();
    const float approval = governance.approval();

    std::map<std::string, std::vector<const PolicyDefinition *>> grouped;
    for (const auto &def : definitions)
      grouped[def.category.empty() ? std::string{"general"} : def.category].push_back(&def);

    UITheme::pushButtonStyle();
    ui::BeginChild("policy_scroll_region", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
    for (const auto &group : grouped)
    {
      ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_HEADER_TEXT);
      ui::TextUnformatted(group.first.c_str());
      ImGui::PopStyleColor();
      ui::Separator();

      for (const PolicyDefinition *def : group.second)
      {
        int level = policyEngine.policyLevel(def->id);
        const int maxLevel = policyEngine.maxLevel(def->id);
        const int currentCost = (level > 0) ? costForLevel(*def, level) : 0;

        ui::TextUnformatted(def->label.c_str());
        if (ui::IsItemHovered())
          ui::SetTooltip("%s", def->description.c_str());

        ui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, level > 0 ? UITheme::COL_YELLOW : UITheme::COL_TEXT_DISABLED);
        ui::Text(level > 0 ? "-%d/mo" : "inactive", currentCost);
        ImGui::PopStyleColor();

        int requestedLevel = level;
        const std::string controlLabel = "##policy_level_" + def->id;

        bool changed = false;
        if (maxLevel <= 1)
        {
          bool enabled = level > 0;
          changed = ui::Checkbox(controlLabel.c_str(), &enabled);
          requestedLevel = enabled ? 1 : 0;
        }
        else
        {
          changed = ui::SliderInt(controlLabel.c_str(), &requestedLevel, 0, maxLevel);
        }

        if (changed)
        {
          PolicyAvailability availability = policyEngine.availability(def->id, requestedLevel, approval);
          if (availability.available)
          {
            const int prevLevel = level;
            policyEngine.setPolicyLevel(def->id, requestedLevel);

            // Push stakeholder reaction notification when policy activation changes.
            const bool wasActive = prevLevel > 0;
            const bool isNowActive = requestedLevel > 0;
            if (!wasActive && isNowActive && !def->stakeholderReactionOn.empty())
            {
              governance.pushStakeholderReaction(def->stakeholderReactionOn, def->category);
            }
            else if (wasActive && !isNowActive && !def->stakeholderReactionOff.empty())
            {
              governance.pushStakeholderReaction(def->stakeholderReactionOff, def->category);
            }
          }
          else
          {
            policyEngine.setPolicyLevel(def->id, level);
          }
        }

        PolicyAvailability nextAvailability = policyEngine.availability(def->id, std::min(maxLevel, level + 1), approval);
        if (!nextAvailability.available && level < maxLevel)
        {
          ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
          ui::TextWrapped("%s", nextAvailability.reason.c_str());
          ImGui::PopStyleColor();
        }

        ui::Spacing();
      }
    }
    ui::EndChild();
    UITheme::popButtonStyle();
  }

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
