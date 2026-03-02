#include "CityIndicesPanel.hxx"

#include <CityIndices.hxx>

#include "imgui.h"

namespace ui = ImGui;

static ImVec4 barColor(float value)
{
  if (value >= 66.f)
    return {0.24f, 0.78f, 0.35f, 1.f};
  if (value >= 33.f)
    return {0.95f, 0.76f, 0.15f, 1.f};
  return {0.90f, 0.25f, 0.20f, 1.f};
}

static const char *trendArrow(float cur, float prev)
{
  const float delta = cur - prev;
  if (delta >  2.f) return "^";
  if (delta < -2.f) return "v";
  return "-";
}

static ImVec4 trendColor(float cur, float prev)
{
  const float delta = cur - prev;
  if (delta >  2.f) return {0.24f, 0.78f, 0.35f, 1.f};
  if (delta < -2.f) return {0.90f, 0.25f, 0.20f, 1.f};
  return {0.50f, 0.54f, 0.58f, 1.f};
}

void CityIndicesPanel::draw() const
{
  const CityIndices &ci = CityIndices::instance();
  const CityIndicesData &cur  = ci.current();
  const CityIndicesData &prev = ci.previous();

  constexpr float panelWidth  = 230.f;
  constexpr float panelHeight = 195.f;
  const ImVec2 panelPos{10.f, 10.f};

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);
  ui::SetNextWindowBgAlpha(0.88f);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
      ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##city_indices", &open, flags))
  {
    ui::End();
    return;
  }

  ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
  ui::Text("City Indices");
  ui::PopStyleColor();

  ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
  ui::Separator();
  ui::PopStyleColor();
  ui::Spacing();

  struct Row
  {
    const char *label;
    float cur;
    float prev;
  };

  const Row rows[] = {
      {"Affordability", cur.affordability, prev.affordability},
      {"Safety",        cur.safety,        prev.safety},
      {"Economy",       cur.jobs,          prev.jobs},
      {"Commute",       cur.commute,       prev.commute},
      {"Pollution",     cur.pollution,     prev.pollution},
  };

  ui::Columns(2, "##idx_cols", false);
  ui::SetColumnWidth(0, 100.f);

  for (const auto &row : rows)
  {
    ui::TextUnformatted(row.label);
    ui::SameLine(0.f, 4.f);
    ui::PushStyleColor(ImGuiCol_Text, trendColor(row.cur, row.prev));
    ui::Text("%s", trendArrow(row.cur, row.prev));
    ui::PopStyleColor();
    ui::NextColumn();

    ui::PushStyleColor(ImGuiCol_PlotHistogram, barColor(row.cur));
    ui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.16f, 0.20f, 1.00f));
    ui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);
    char overlay[8];
    snprintf(overlay, sizeof(overlay), "%.0f", row.cur);
    ui::ProgressBar(row.cur / 100.f, ImVec2(-1.f, 14.f), overlay);
    ui::PopStyleVar();
    ui::PopStyleColor(2);
    ui::NextColumn();
  }

  ui::Columns(1);
  ui::End();
}
