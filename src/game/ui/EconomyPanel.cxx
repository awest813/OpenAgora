#include "EconomyPanel.hxx"
#include "UITheme.hxx"

#include <BudgetSystem.hxx>
#include <SimulationContext.hxx>

#include "imgui.h"

namespace ui = ImGui;

namespace
{
// Returns a colour reflecting how healthy a value is.
// `higherIsBetter` controls the direction of the scale.
ImVec4 statusColour(float value, bool higherIsBetter)
{
  const float normalised = higherIsBetter ? value : (100.f - value);
  return UITheme::statusColor(normalised);
}
} // namespace

void EconomyPanel::draw() const
{
  const SimulationContextData &ctx = SimulationContext::instance().data();
  const BudgetSystem &budget = BudgetSystem::instance();

  constexpr float panelWidth  = 300.f;
  constexpr float panelHeight = 290.f;
  const ImVec2 panelPos{6.f, 700.f};

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.f, 5.f));

  ui::SetNextWindowPos(panelPos, ImGuiCond_Always);
  ui::SetNextWindowSize({panelWidth, panelHeight}, ImGuiCond_Always);

  const ImGuiWindowFlags flags =
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration;

  bool open = true;
  if (!ui::Begin("##economy_panel", &open, flags))
  {
    ui::End();
    ImGui::PopStyleVar();
    UITheme::popPanelStyle();
    return;
  }

  UITheme::sectionHeader("  Economy");

  ui::Text("Month: %d", ctx.month);

  const float balance = budget.currentBalance();
  ui::TextColored(balance >= 0.f ? UITheme::COL_GREEN : UITheme::COL_RED,
                  "Balance: %.0f", balance);
  ui::Text("Tax efficiency: %.2fx", ctx.taxEfficiency);
  ui::Text("Growth modifier: %.2fx", ctx.growthRateModifier);

  ui::Separator();
  UITheme::sectionHeader("  Economy Depth");

  ui::TextColored(statusColour(ctx.unemploymentPressure, /*higherIsBetter=*/false),
                  "Unemployment pressure: %.0f", ctx.unemploymentPressure);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = more unemployment.  Below 40 is healthy.\n"
                   "Drives wage pressure and commercial growth.");

  ui::TextColored(statusColour(ctx.wagePressure, /*higherIsBetter=*/true),
                  "Wage pressure: %.0f", ctx.wagePressure);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = stronger labour demand.  Benefits workers;\n"
                   "may slow commercial expansion.");

  ui::TextColored(statusColour(ctx.businessConfidence, /*higherIsBetter=*/true),
                  "Business confidence: %.0f", ctx.businessConfidence);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = stronger investment appetite.\n"
                   "Above 60 accelerates commercial zone growth.");

  ui::TextColored(statusColour(ctx.debtStress, /*higherIsBetter=*/false),
                  "Debt stress: %.0f", ctx.debtStress);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = greater fiscal strain.\n"
                   "Above 60 reduces business confidence and industrial growth.");

  ui::Separator();
  UITheme::sectionHeader("  Service Strain");

  ui::TextColored(statusColour(ctx.transitReliability, /*higherIsBetter=*/true),
                  "Transit reliability: %.0f", ctx.transitReliability);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = more reliable service.  Driven by\n"
                   "road coverage and commute index.");

  ui::TextColored(statusColour(ctx.safetyCapacityLoad, /*higherIsBetter=*/false),
                  "Safety capacity load: %.0f", ctx.safetyCapacityLoad);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = emergency services more overloaded.\n"
                   "Build more emergency facilities to reduce.");

  ui::TextColored(statusColour(ctx.educationAccessStress, /*higherIsBetter=*/false),
                  "Education stress: %.0f", ctx.educationAccessStress);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = poorer access to schools.\n"
                   "Build schools to reduce.");

  ui::TextColored(statusColour(ctx.healthAccessStress, /*higherIsBetter=*/false),
                  "Health stress: %.0f", ctx.healthAccessStress);
  if (ui::IsItemHovered())
    ui::SetTooltip("Higher = poorer access to health care.\n"
                   "Build hospitals and water infrastructure to reduce.");

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
