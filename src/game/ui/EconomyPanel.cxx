#include "EconomyPanel.hxx"
#include "UITheme.hxx"

#include <BudgetSystem.hxx>
#include <SimulationContext.hxx>

#include "imgui.h"

namespace ui = ImGui;

void EconomyPanel::draw() const
{
  const SimulationContextData &ctx = SimulationContext::instance().data();
  const BudgetSystem &budget = BudgetSystem::instance();

  constexpr float panelWidth = 300.f;
  constexpr float panelHeight = 220.f;
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
  ui::Text("Balance: %.0f", budget.currentBalance());
  ui::Text("Tax efficiency: %.2fx", ctx.taxEfficiency);
  ui::Text("Growth modifier: %.2fx", ctx.growthRateModifier);

  ui::Separator();
  ui::Text("Unemployment pressure: %.0f", ctx.unemploymentPressure);
  ui::Text("Wage pressure: %.0f", ctx.wagePressure);
  ui::Text("Business confidence: %.0f", ctx.businessConfidence);
  ui::Text("Debt stress: %.0f", ctx.debtStress);

  ui::Separator();
  ui::Text("Transit reliability: %.0f", ctx.transitReliability);
  ui::Text("Safety load: %.0f", ctx.safetyCapacityLoad);
  ui::Text("Education stress: %.0f", ctx.educationAccessStress);
  ui::Text("Health stress: %.0f", ctx.healthAccessStress);

  ui::End();
  ImGui::PopStyleVar();
  UITheme::popPanelStyle();
}
