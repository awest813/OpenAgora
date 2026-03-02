#include "PauseMenu.hxx"

#include "SettingsMenu.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "SignalMediator.hxx"

namespace ui = ImGui;

void PauseMenu::draw() const
{
  ImVec2 windowSize(320, 420);
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();

  const auto &layout = uiManager.getLayouts()["PauseMenuButtons"];
  ui::SetNextWindowPos(ImVec2((screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2));
  ui::SetNextWindowSize(windowSize);

  const ImVec2 buttonSize(240, 44);
  const float buttonSpacing = 12.f;
  const float sideMargin = (windowSize.x - buttonSize.x) / 2;

  ui::PushFont(layout.font);
  ui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(sideMargin, 20));
  ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, buttonSpacing));

  bool open = true;
  ui::BeginCt("PauseMenu", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  {
    auto textWidth = ui::CalcTextSize("Paused").x;
    ui::SetCursorPosX((windowSize.x - textWidth) * 0.5f);
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
    ui::Text("Paused");
    ui::PopStyleColor();
  }

  ui::Spacing();
  ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
  ui::Separator();
  ui::PopStyleColor();
  ui::Spacing();

  if (ui::ButtonCt("Settings", buttonSize))
  {
    Settings::instance().writeFile();
    uiManager.openMenu<SettingsMenu>();
  }

  if (ui::ButtonCt("New Game", buttonSize))
  {
    SignalMediator::instance().signalNewGame.emit(true);
  }

  if (ui::ButtonCt("Save Game", buttonSize))
  {
    SignalMediator::instance().signalSaveGame.emit("save.cts");
  }

  if (ui::ButtonCt("Load Game", buttonSize))
  {
    SignalMediator::instance().signalLoadGame.emit("save.cts");
  }

  ui::Spacing();

  ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.35f, 1.00f));
  if (ui::ButtonCt("Quit Game", buttonSize))
  {
    SignalMediator::instance().signalQuitGame.emit();
  }
  ui::PopStyleColor();

  ui::PopFont();
  ui::PopStyleVar(2);

  ui::End();
}