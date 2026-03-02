#include "GovernancePanel.hxx"

#include <GovernanceSystem.hxx>
#include <PolicyEngine.hxx>

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace ui = ImGui;

namespace
{
ImVec4 approvalColor(float approval)
{
  if (approval >= 66.f)
    return {0.24f, 0.78f, 0.35f, 1.f};
  if (approval >= 33.f)
    return {0.95f, 0.76f, 0.15f, 1.f};
  return {0.90f, 0.25f, 0.20f, 1.f};
}

const char *statusLabel(const GovernanceSystem &governance)
{
  if (governance.lostElection())
    return "Soft fail";
  if (governance.policyConstrained())
    return "Constrained";
  return "Stable";
}

ImVec4 statusColor(const GovernanceSystem &governance)
{
  if (governance.lostElection())
    return {0.90f, 0.25f, 0.20f, 1.f};
  if (governance.policyConstrained())
    return {0.95f, 0.76f, 0.15f, 1.f};
  return {0.24f, 0.78f, 0.35f, 1.f};
}
} // namespace

void GovernancePanel::draw() const
{
  GovernanceSystem &governance = GovernanceSystem::instance();

  constexpr float panelWidth = 300.f;
  constexpr float panelHeight = 240.f;
  const ImVec2 panelPos{10.f, 215.f};

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);
  ui::SetNextWindowBgAlpha(0.88f);

  const ImGuiWindowFlags panelFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##governance_panel", &open, panelFlags))
  {
    ui::End();
    return;
  }

  ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
  ui::Text("Governance");
  ui::PopStyleColor();

  ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
  ui::Separator();
  ui::PopStyleColor();
  ui::Spacing();

  ui::Text("Public approval");
  ui::PushStyleColor(ImGuiCol_PlotHistogram, approvalColor(governance.approval()));
  ui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.16f, 0.20f, 1.00f));
  ui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
  char overlay[16];
  snprintf(overlay, sizeof(overlay), "%.0f / 100", governance.approval());
  ui::ProgressBar(governance.approval() / 100.f, ImVec2(-1.f, 16.f), overlay);
  ui::PopStyleVar();
  ui::PopStyleColor(2);

  ui::Spacing();
  ui::Text("Status:");
  ui::SameLine();
  ui::TextColored(statusColor(governance), "%s", statusLabel(governance));

  const int monthsToCheckpoint = governance.monthsUntilCheckpoint();
  if (monthsToCheckpoint >= 0)
  {
    ui::TextColored(ImVec4(0.50f, 0.54f, 0.58f, 1.00f), "Council checkpoint in: %d month%s",
                    monthsToCheckpoint, monthsToCheckpoint == 1 ? "" : "s");
  }

  if (governance.lostElection())
  {
    ui::Spacing();
    ui::TextColored({0.95f, 0.35f, 0.25f, 1.f}, "You lost re-election. Sandbox continues.");
  }

  ui::Spacing();
  ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
  ui::Separator();
  ui::PopStyleColor();

  ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.54f, 0.58f, 1.00f));
  ui::Text("Recent events");
  ui::PopStyleColor();

  const auto &notifications = governance.recentNotifications();
  if (notifications.empty())
  {
    ui::TextColored(ImVec4(0.40f, 0.42f, 0.46f, 1.00f), "No civic events yet.");
  }
  else
  {
    const int from = std::max(0, static_cast<int>(notifications.size()) - 4);
    for (int i = from; i < static_cast<int>(notifications.size()); ++i)
    {
      const GovernanceNotification &note = notifications[i];
      ui::TextWrapped("M%d: %s", note.month, note.text.c_str());
    }
  }

  if (governance.checkpointPending())
  {
    ui::OpenPopup("Council Checkpoint");
  }

  if (ui::BeginPopupModal("Council Checkpoint", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
    ui::Text("Approval rating: %.0f / 100", governance.approval());
    ui::PopStyleColor();

    ui::Separator();

    const CityIndicesData &indices = governance.lastIndices();
    ui::BulletText("Affordability: %.0f", indices.affordability);
    ui::BulletText("Safety: %.0f", indices.safety);
    ui::BulletText("Jobs/Economy: %.0f", indices.jobs);
    ui::BulletText("Commute: %.0f", indices.commute);
    ui::BulletText("Pollution: %.0f", indices.pollution);

    ui::Separator();
    ui::Text("Policy pledges (choose up to %d):", governance.maxSelectablePolicies());

    for (const auto &policy : governance.policyOptions())
    {
      bool selected = policy.selected;
      if (!policy.enabled)
      {
        ui::PushStyleVar(ImGuiStyleVar_Alpha, ui::GetStyle().Alpha * 0.5f);
      }

      if (ui::Checkbox(policy.label.c_str(), &selected))
      {
        if (policy.enabled)
        {
          governance.setPolicySelection(policy.id, selected);
        }
      }
      if (ui::IsItemHovered())
      {
        ui::SetTooltip("%s", policy.description.c_str());
      }

      if (!policy.enabled)
      {
        ui::PopStyleVar();
      }
    }

    ui::Separator();
    if (governance.lostElection())
    {
      ui::TextColored({0.95f, 0.35f, 0.25f, 1.f}, "Election lost. Sandbox mode remains active.");
    }
    else if (governance.policyConstrained())
    {
      ui::TextColored({0.95f, 0.76f, 0.15f, 1.f}, "Low approval: policy options are constrained.");
    }
    else
    {
      ui::TextColored({0.24f, 0.78f, 0.35f, 1.f}, "Re-election secured.");
    }

    if (ui::ButtonCt("Continue", ImVec2(220.f, 36.f)))
    {
      PolicyEngine &policyEngine = PolicyEngine::instance();
      for (const auto &policy : governance.policyOptions())
      {
        policyEngine.setActive(policy.id, policy.selected);
      }

      governance.acknowledgeCheckpoint();
      ui::CloseCurrentPopup();
    }

    ui::EndPopup();
  }

  ui::End();
}
