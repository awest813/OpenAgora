#include "CityIndicesPanel.hxx"
#include "UITheme.hxx"

#include <CityIndices.hxx>

#include "imgui.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include <cstdio>

namespace ui = ImGui;

// ── draw() ────────────────────────────────────────────────────────────────────

void CityIndicesPanel::draw() const
{
  const CityIndices &ci   = CityIndices::instance();
  const CityIndicesData &cur  = ci.current();
  const CityIndicesData &prev = ci.previous();

  constexpr float panelWidth  = 240.f;
  constexpr float panelHeight = 212.f;
  const ImVec2 panelPos{6.f, 6.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##city_indices", &open, flags))
  {
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  UITheme::sectionHeader("  City Indices");

  struct Row
  {
    const char *icon;
    const char *label;
    const char *tooltip;
    float cur;
    float prev;
  };

  const Row rows[] = {
      {"$", "Affordability", "Housing cost vs. density", cur.affordability, prev.affordability},
      {"S", "Safety",        "Inverted crime + fire hazard", cur.safety,        prev.safety},
      {"E", "Economy",       "Commercial/industrial capacity relative to residents", cur.jobs,          prev.jobs},
      {"C", "Commute",       "Road infrastructure relative to map size", cur.commute,       prev.commute},
      {"P", "Pollution",     "Inverted pollution (100 = pristine)", cur.pollution,     prev.pollution},
  };

  const float labelColW = 94.f;
  const float barH = 12.f;

  for (const auto &row : rows)
  {
    // Label + trend on the left
    ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    ui::Text("%s %-12s", row.icon, row.label);
    if (ui::IsItemHovered())
      ui::SetTooltip("%s", row.tooltip);
    ImGui::PopStyleColor();

    // Trend arrow
    const char *arrow  = UITheme::trendArrow(row.cur, row.prev);
    float       delta  = row.cur - row.prev;
    ImVec4      arrowClr = (delta > 2.f)  ? UITheme::COL_GREEN :
                           (delta < -2.f) ? UITheme::COL_RED
                                          : UITheme::COL_TEXT_DISABLED;
    ui::SameLine(labelColW);
    ui::PushStyleColor(ImGuiCol_Text, arrowClr);
    ui::TextUnformatted(arrow);
    ui::PopStyleColor();

    // Progress bar on the same line
    ui::SameLine(labelColW + 20.f);
    ui::PushStyleColor(ImGuiCol_PlotHistogram,         UITheme::statusColor(row.cur));
    ui::PushStyleColor(ImGuiCol_FrameBg,               ImVec4(0.15f, 0.18f, 0.24f, 1.f));

    char overlay[8];
    snprintf(overlay, sizeof(overlay), "%.0f", row.cur);
    ui::ProgressBar(row.cur / 100.f, ImVec2(-1.f, barH), overlay);
    if (ui::IsItemHovered())
      ui::SetTooltip("%s", row.tooltip);
    ui::PopStyleColor(2);
  }

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
