#include "EventLogPanel.hxx"
#include "UITheme.hxx"

#include <GovernanceSystem.hxx>

#include "imgui.h"

#include <algorithm>

namespace ui = ImGui;

void EventLogPanel::draw() const
{
  const GovernanceSystem &governance = GovernanceSystem::instance();
  const auto &notes = governance.recentNotifications();

  constexpr float panelWidth = 420.f;
  constexpr float panelHeight = 220.f;
  const ImVec2 panelPos{320.f, 700.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##event_log_panel", &open, flags))
  {
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  UITheme::sectionHeader("  Event Log");

  if (notes.empty())
  {
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    ui::TextUnformatted("  No events recorded yet.");
    ImGui::PopStyleColor();
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  ui::BeginChild("event_log_scroll", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (const GovernanceNotification &note : notes)
  {
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
    ui::Text("Month %d", note.month);
    ui::PopStyleColor();
    ui::SameLine(0.f, 8.f);
    ui::TextWrapped("%s", note.text.c_str());
    ui::Separator();
  }
  ui::EndChild();

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
