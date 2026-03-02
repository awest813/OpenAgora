#include "GameTimeMenu.hxx"

#include "SettingsMenu.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "../services/GameClock.hxx"

namespace ui = ImGui;

void GameTimeMenu::draw() const
{
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();

  const auto &layout = uiManager.getLayouts()["BuildMenuButtons"];
  const ImVec2 buttonSize(36, 32);
  const float spacing = 6.f;
  const float pad = 8.f;
  ImVec2 windowSize((buttonSize.x + spacing) * 4 - spacing + pad * 2, buttonSize.y + pad * 2);
  ImVec2 pos(screenSize.x - windowSize.x - 8, 8);

  ui::PushFont(layout.font);
  ui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{pad, pad});
  ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{spacing, 0});

  ui::SetNextWindowPos(pos);
  ui::SetNextWindowSize(windowSize);
  ui::SetNextWindowBgAlpha(0.85f);

  bool open = true;
  ui::Begin("##gametimemenu", &open,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration);

  ImGuiWindow *window = ImGui::GetCurrentWindow();

  window->DrawList->AddRect(window->Pos, window->Pos + windowSize,
                            ImColor(0.22f, 0.28f, 0.36f, 0.40f), 6.0f, 0, 1.0f);

  const float speedFactor = GameClock::instance().getGameClockSpeed();
  auto is_interval = [&](float val) { return fabs(speedFactor - val) < 0.1f ? ImGuiButtonFlags_ForcePressed : 0; };

  if (ui::ButtonCtEx("||", buttonSize, is_interval(0.f)))
  {
    GameClock::instance().setGameClockSpeed(0.0f);
  }

  ui::SameLine();
  if (ui::ButtonCtEx(">", buttonSize, is_interval(1.f)))
  {
    GameClock::instance().setGameClockSpeed(1.0f);
  }

  ui::SameLine();
  if (ui::ButtonCtEx(">>", buttonSize, is_interval(2.f)))
  {
    GameClock::instance().setGameClockSpeed(2.0f);
  }

  ui::SameLine();
  if (ui::ButtonCtEx(">>>", buttonSize, is_interval(8.f)))
  {
    GameClock::instance().setGameClockSpeed(8.0f);
  }

  ui::PopFont();
  ui::PopStyleVar(2);

  ui::End();
}