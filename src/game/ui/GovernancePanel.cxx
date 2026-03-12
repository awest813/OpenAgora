#include "GovernancePanel.hxx"
#include "UITheme.hxx"

#include <GovernanceSystem.hxx>
#include <PolicyEngine.hxx>

#include "imgui.h"

#include <algorithm>
#include <cstdio>

namespace ui = ImGui;

// ── draw() ────────────────────────────────────────────────────────────────────

void GovernancePanel::draw() const
{
  GovernanceSystem &governance = GovernanceSystem::instance();

  constexpr float panelWidth  = 260.f;
  // 270 px: extra 20 px over the original 250 accommodates the pledge sidebar row,
  // win banner, and consecutive-checkpoint streak display.
  constexpr float panelHeight = 270.f;
  // sits just below the CityIndicesPanel (which ends at ~218)
  const ImVec2 panelPos{6.f, 224.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags panelFlags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##governance_panel", &open, panelFlags))
  {
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  UITheme::sectionHeader("  Governance");

  // ── Election win banner ────────────────────────────────────────────────────
  if (governance.wonElection())
  {
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_GREEN);
    ui::TextUnformatted("  ★ Term Secured – three-term majority");
    if (ui::IsItemHovered())
      ui::SetTooltip("You have won three consecutive council elections.\nYour mandate is strong.");
    ui::PopStyleColor();
    ui::Spacing();
  }

  // ── Approval bar ──────────────────────────────────────────────────────────
  {
    float approval = governance.approval();

    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    ui::TextUnformatted("Public Approval");
    ImGui::PopStyleColor();

    ui::PushStyleColor(ImGuiCol_PlotHistogram, UITheme::statusColor(approval));
    ui::PushStyleColor(ImGuiCol_FrameBg,       ImVec4(0.15f, 0.18f, 0.24f, 1.f));

    char overlay[20];
    snprintf(overlay, sizeof(overlay), "%.0f / 100", approval);
    ui::ProgressBar(approval / 100.f, ImVec2(-1.f, 14.f), overlay);
    if (ui::IsItemHovered())
      ui::SetTooltip("Overall citizen satisfaction.\nDetermines policy options at next election.");
    ui::PopStyleColor(2);
  }

  // ── Status badges ─────────────────────────────────────────────────────────
  {
    const bool lost        = governance.lostElection();
    const bool constrained = governance.policyConstrained();
    const int  months      = governance.monthsUntilCheckpoint();

    ImVec4 statusClr = lost ? UITheme::COL_RED
                     : constrained ? UITheme::COL_YELLOW
                                   : UITheme::COL_GREEN;

    const char *statusTxt = lost        ? "Status: Election Lost"
                          : constrained ? "Status: Constrained"
                                        : "Status: Stable";

    ui::PushStyleColor(ImGuiCol_Text, statusClr);
    ui::TextUnformatted(statusTxt);
    if (ui::IsItemHovered())
    {
      if (lost)
        ui::SetTooltip("Election lost. Sandbox mode remains active.");
      else if (constrained)
        ui::SetTooltip("Low approval. Policy options are severely limited.");
      else
        ui::SetTooltip("High approval. All policy options are available.");
    }
    ui::PopStyleColor();

    if (months >= 0)
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
      ui::Text("Council checkpoint in: %d month%s", months, months == 1 ? "" : "s");
      if (ui::IsItemHovered())
        ui::SetTooltip("Months until the next council election where\napproval rating determines your continued mandate.");
      ui::PopStyleColor();
    }

    // ── Consecutive checkpoint streak ─────────────────────────────────────
    const int streak = governance.consecutiveSuccessfulCheckpoints();
    if (streak > 0)
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
      ui::Text("Consecutive wins: %d / %d", streak, 3);
      if (ui::IsItemHovered())
        ui::SetTooltip("Win %d consecutive council checkpoints (approval >= soft-fail threshold)\nto secure your electoral mandate.", 3);
      ui::PopStyleColor();
    }

    // ── Active pledge ──────────────────────────────────────────────────────
    if (governance.hasPledge())
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ORANGE);
      ui::TextWrapped("Pledge: %s", governance.activePledge().description.c_str());
      ui::PopStyleColor();
    }
  }

  ui::Spacing();
  ui::Separator();

  // ── Recent events ─────────────────────────────────────────────────────────
  ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_HEADER_TEXT);
  ui::TextUnformatted("Recent Events");
  ImGui::PopStyleColor();

  const auto &notifications = governance.recentNotifications();
  if (notifications.empty())
  {
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    ui::TextUnformatted("  No civic events yet.");
    ui::PopStyleColor();
  }
  else
  {
    const int from = std::max(0, static_cast<int>(notifications.size()) - 3);
    for (int i = from; i < static_cast<int>(notifications.size()); ++i)
    {
      const GovernanceNotification &note = notifications[i];

      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
      ui::Text("M%d", note.month);
      ui::PopStyleColor();

      ui::SameLine(0.f, 4.f);
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_PRIMARY);
      ui::TextWrapped("%s", note.text.c_str());
      ui::PopStyleColor();
    }
  }

  // ── Pending event choices ─────────────────────────────────────────────────
  if (governance.hasPendingEventChoice())
  {
    const GovernanceEventDefinition *pending = governance.pendingEventChoice();
    if (pending)
    {
      ui::Spacing();
      ui::Separator();
      ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ORANGE);
      ui::Text("Decision Required: %s", pending->id.c_str());
      ImGui::PopStyleColor();
      if (!pending->notification.empty())
      {
        ui::TextWrapped("%s", pending->notification.c_str());
      }

      UITheme::pushButtonStyle();
      for (const auto &choice : pending->choices)
      {
        if (ui::Button(choice.label.c_str(), ImVec2(-1.f, 0.f)))
        {
          governance.choosePendingEventOption(choice.id);
        }
        if (ui::IsItemHovered())
        {
          if (!choice.description.empty())
            ui::SetTooltip("%s", choice.description.c_str());
        }
      }
      UITheme::popButtonStyle();
    }
  }

  // ── Council checkpoint modal ──────────────────────────────────────────────
  if (governance.checkpointPending())
    ui::OpenPopup("Council Checkpoint");

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.f, 14.f));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
  ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(0.10f, 0.12f, 0.16f, 0.98f));

  if (ui::BeginPopupModal("Council Checkpoint", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
  {
    UITheme::sectionHeader("Council Checkpoint");

    ui::Text("Approval rating: %.0f / 100", governance.approval());

    ui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_HEADER_TEXT);
    ui::TextUnformatted("City Performance");
    ImGui::PopStyleColor();

    const CityIndicesData &indices = governance.lastIndices();
    const struct { const char *n; float v; } stats[] = {
        {"Affordability", indices.affordability},
        {"Safety",        indices.safety},
        {"Jobs / Economy",indices.jobs},
        {"Commute",       indices.commute},
        {"Pollution",     indices.pollution},
    };

    for (const auto &s : stats)
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::statusColor(s.v));
      ui::BulletText("%-16s  %.0f", s.n, s.v);
      ui::PopStyleColor();
    }

    ui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_HEADER_TEXT);
    ui::Text("Policy Pledges  (choose up to %d):", governance.maxSelectablePolicies());
    ImGui::PopStyleColor();

    UITheme::pushButtonStyle();
    for (const auto &policy : governance.policyOptions())
    {
      bool selected = policy.selected;
      if (!policy.enabled)
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ui::GetStyle().Alpha * 0.45f);

      if (ui::Checkbox(policy.label.c_str(), &selected))
      {
        if (policy.enabled)
          governance.setPolicySelection(policy.id, selected);
      }
      if (ui::IsItemHovered())
        ui::SetTooltip("%s", policy.description.c_str());

      if (!policy.enabled)
        ImGui::PopStyleVar();
    }
    UITheme::popButtonStyle();

    // ── Next-term pledge selection ──────────────────────────────────────
    ui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_HEADER_TEXT);
    ui::TextUnformatted("Term Pledge (optional)");
    ImGui::PopStyleColor();
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    ui::TextWrapped("Commit to one measurable goal. Keeping it earns approval; breaking it costs trust.");
    ui::PopStyleColor();

    const auto pledges = governance.availablePledges();
    const std::string currentPledgeId = governance.hasPledge() ? governance.activePledge().id : "";

    UITheme::pushButtonStyle();
    // "No pledge" option
    {
      const bool noneSelected = currentPledgeId.empty();
      if (noneSelected)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.30f, 0.42f, 1.f));
      if (ui::Button("No pledge", ImVec2(-1.f, 0.f)))
        governance.clearPledge();
      if (noneSelected)
        ImGui::PopStyleColor();
    }
    for (const auto &pledge : pledges)
    {
      const bool isActive = (currentPledgeId == pledge.id);
      if (isActive)
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.30f, 0.42f, 1.f));
      if (ui::Button(pledge.description.c_str(), ImVec2(-1.f, 0.f)))
        governance.setPledge(pledge.id);
      if (ui::IsItemHovered())
      {
        ui::SetTooltip("+%.0f approval if kept,  -%.0f if broken at next checkpoint.",
                       pledge.bonusApproval, pledge.penaltyApproval);
      }
      if (isActive)
        ImGui::PopStyleColor();
    }
    UITheme::popButtonStyle();

    ui::Separator();

    // Result banner
    if (governance.lostElection())
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_RED);
      ui::TextUnformatted("Election lost. Sandbox mode remains active.");
      ui::PopStyleColor();
    }
    else if (governance.wonElection())
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_GREEN);
      ui::TextUnformatted("★ Electoral victory secured — three-term mandate!");
      ui::PopStyleColor();
    }
    else if (governance.policyConstrained())
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_YELLOW);
      ui::TextUnformatted("Low approval: policy options are constrained.");
      ui::PopStyleColor();
    }
    else
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_GREEN);
      ui::TextUnformatted("Re-election secured.");
      ui::PopStyleColor();
    }

    ui::Spacing();
    UITheme::pushButtonStyle();
    if (ui::Button("Continue", ImVec2(240.f, 0.f)))
    {
      PolicyEngine &policyEngine = PolicyEngine::instance();
      for (const auto &policy : governance.policyOptions())
        policyEngine.setActive(policy.id, policy.selected);

      governance.acknowledgeCheckpoint();
      ui::CloseCurrentPopup();
    }
    UITheme::popButtonStyle();

    ui::EndPopup();
  }

  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
