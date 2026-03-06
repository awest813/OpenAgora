#include "PauseMenu.hxx"
#include "UITheme.hxx"

#include "SettingsMenu.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "SignalMediator.hxx"

namespace ui = ImGui;

void PauseMenu::draw() const
{
  constexpr float winW = 320.f;
  constexpr float winH = 380.f;
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();
  const auto &layout = uiManager.getLayouts()["PauseMenuButtons"];

  ui::SetNextWindowPos(
      ImVec2((screenSize.x - winW) * 0.5f, (screenSize.y - winH) * 0.5f));
  ui::SetNextWindowSize(ImVec2(winW, winH));

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 20.f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, 10.f));

  bool open = true;
  ui::BeginCt("PauseMenu", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  // ── Header ────────────────────────────────────────────────────────────────
  ui::PushFont(layout.font);

  {
    const char *title = "PAUSED";
    float tw = ui::CalcTextSize(title).x;
    ui::SetCursorPosX((winW - tw) * 0.5f);
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_ACCENT);
    ui::TextUnformatted(title);
    ui::PopStyleColor();
  }
  ui::PopFont();

  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  // ── Buttons ───────────────────────────────────────────────────────────────
  UITheme::pushButtonStyle();

  const ImVec2 btnSize(winW - 48.f, 40.f);

  auto centeredButton = [&](const char *label, const char *tooltip = nullptr) -> bool {
    bool pressed = ui::ButtonCt(label, btnSize);
    if (tooltip && ui::IsItemHovered())
      ui::SetTooltip("%s", tooltip);
    return pressed;
  };

  if (centeredButton("Settings", "Configure game options"))
  {
    Settings::instance().writeFile();
    uiManager.openMenu<SettingsMenu>();
  }

  if (centeredButton("New Game", "Abandon current city and start fresh"))
    SignalMediator::instance().signalNewGame.emit(true);

  if (centeredButton("Save Game", "Save current city progress"))
    SignalMediator::instance().signalSaveGame.emit("save.cts");

  if (centeredButton("Load Game", "Load a saved city"))
    SignalMediator::instance().signalLoadGame.emit("save.cts");

  // Divider before destructive action
  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  ui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.10f, 0.10f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.15f, 0.15f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.08f, 0.08f, 1.f));
  if (centeredButton("Quit Game", "Exit to desktop"))
    SignalMediator::instance().signalQuitGame.emit();
  ui::PopStyleColor(3);

  UITheme::popButtonStyle();

  ImGui::PopStyleVar(2);
  UITheme::popPanelStyle();

  ui::End();
}
