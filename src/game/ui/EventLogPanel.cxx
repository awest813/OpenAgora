#include "EventLogPanel.hxx"
#include "UITheme.hxx"

#include <GovernanceSystem.hxx>

#include "imgui.h"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

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

  // ── Filters ───────────────────────────────────────────────────────────────
  static std::string selectedCategory = "all";
  static int minSeverity = 0; // 0 = all, 1..3
  static int monthWindow = 0; // 0 = all, 6/12/24 = rolling window

  std::set<std::string> categorySet;
  categorySet.insert("all");
  for (const auto &note : notes)
    categorySet.insert(note.category);

  if (!categorySet.count(selectedCategory))
    selectedCategory = "all";

  ui::TextUnformatted("Category");
  ui::SameLine(68.f);
  if (ui::BeginCombo("##event_category_filter", selectedCategory.c_str()))
  {
    for (const auto &category : categorySet)
    {
      const bool selected = (selectedCategory == category);
      if (ui::Selectable(category.c_str(), selected))
        selectedCategory = category;
      if (selected)
        ui::SetItemDefaultFocus();
    }
    ui::EndCombo();
  }

  const char *severityLabel = (minSeverity == 0) ? "all" : (minSeverity == 1) ? "low+" : (minSeverity == 2) ? "medium+" : "high";
  ui::SameLine();
  if (ui::BeginCombo("##event_severity_filter", severityLabel))
  {
    if (ui::Selectable("all", minSeverity == 0)) minSeverity = 0;
    if (ui::Selectable("low+", minSeverity == 1)) minSeverity = 1;
    if (ui::Selectable("medium+", minSeverity == 2)) minSeverity = 2;
    if (ui::Selectable("high", minSeverity == 3)) minSeverity = 3;
    ui::EndCombo();
  }

  const char *windowLabel = (monthWindow == 0) ? "all months"
                           : (monthWindow == 6) ? "last 6"
                           : (monthWindow == 12) ? "last 12"
                           : "last 24";
  ui::SameLine();
  if (ui::BeginCombo("##event_window_filter", windowLabel))
  {
    if (ui::Selectable("all months", monthWindow == 0)) monthWindow = 0;
    if (ui::Selectable("last 6", monthWindow == 6)) monthWindow = 6;
    if (ui::Selectable("last 12", monthWindow == 12)) monthWindow = 12;
    if (ui::Selectable("last 24", monthWindow == 24)) monthWindow = 24;
    ui::EndCombo();
  }

  ui::Separator();

  const int currentMonth = governance.monthCounter();
  int shown = 0;
  ui::BeginChild("event_log_scroll", ImVec2(0.f, 0.f), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
  for (const GovernanceNotification &note : notes)
  {
    if (selectedCategory != "all" && note.category != selectedCategory)
      continue;
    if (minSeverity > 0 && note.severity < minSeverity)
      continue;
    if (monthWindow > 0 && note.month < std::max(0, currentMonth - monthWindow + 1))
      continue;

    ++shown;
    const ImVec4 sevColor = (note.severity >= 3) ? UITheme::COL_RED
                          : (note.severity == 2) ? UITheme::COL_YELLOW
                                                 : UITheme::COL_ACCENT;

    ui::PushStyleColor(ImGuiCol_Text, sevColor);
    ui::Text("M%d  [%s]", note.month, note.category.c_str());
    ui::PopStyleColor();
    ui::TextWrapped("%s", note.text.c_str());
    ui::Separator();
  }

  if (shown == 0)
  {
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    ui::TextUnformatted("No events match current filters.");
    ImGui::PopStyleColor();
  }

  ui::EndChild();

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
