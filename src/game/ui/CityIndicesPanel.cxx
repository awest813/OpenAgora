#include "CityIndicesPanel.hxx"

#include <CityIndices.hxx>

#include "imgui.h"

namespace ui = ImGui;

// ── Helpers ───────────────────────────────────────────────────────────────────

static ImVec4 barColor(float value)
{
  if (value >= 66.f)
    return {0.24f, 0.78f, 0.35f, 1.f}; // green
  if (value >= 33.f)
    return {0.95f, 0.76f, 0.15f, 1.f}; // yellow
  return {0.90f, 0.25f, 0.20f, 1.f};   // red
}

// Returns a UTF-8 arrow character comparing current with previous.
static const char *trendArrow(float cur, float prev)
{
  const float delta = cur - prev;
  if (delta >  2.f) return "^";
  if (delta < -2.f) return "v";
  return "-";
}

// ── draw() ────────────────────────────────────────────────────────────────────

void CityIndicesPanel::draw() const
{
  const CityIndices &ci = CityIndices::instance();
  const CityIndicesData &cur  = ci.current();
  const CityIndicesData &prev = ci.previous();

  constexpr float panelWidth  = 220.f;
  constexpr float panelHeight = 175.f;
  const ImVec2 panelPos{4.f, 4.f}; // top-left, with a small margin

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);
  ui::SetNextWindowBgAlpha(0.82f);

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

  ui::TextDisabled("City Indices");
  ui::Separator();

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
  ui::SetColumnWidth(0, 92.f);

  for (const auto &row : rows)
  {
    // left column: label + trend arrow
    ui::TextUnformatted(row.label);
    ui::SameLine(0.f, 4.f);
    ui::TextDisabled("%s", trendArrow(row.cur, row.prev));
    ui::NextColumn();

    // right column: coloured progress bar
    ui::PushStyleColor(ImGuiCol_PlotHistogram, barColor(row.cur));
    char overlay[8];
    snprintf(overlay, sizeof(overlay), "%.0f", row.cur);
    ui::ProgressBar(row.cur / 100.f, ImVec2(-1.f, 14.f), overlay);
    ui::PopStyleColor();
    ui::NextColumn();
  }

  ui::Columns(1);
  ui::End();
}
