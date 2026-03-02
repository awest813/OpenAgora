#include "GameTimeMenu.hxx"
#include "UITheme.hxx"

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
  auto &uiManager   = UIManager::instance();

  // Four speed buttons + Pause button
  const ImVec2 buttonSize(36, 28);
  const int    spacing   = 4;
  const int    numBtns   = 4;
  const float  winW      = (buttonSize.x + spacing) * numBtns + spacing * 2;
  const float  winH      = buttonSize.y + spacing * 2 + 4;
  const ImVec2 pos(screenSize.x - winW - 4, 4);

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(spacing, spacing));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(spacing, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(4, 4));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f);

  ImGui::PushStyleColor(ImGuiCol_Button,        UITheme::COL_BTN);
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::COL_BTN_HOVER);
  ImGui::PushStyleColor(ImGuiCol_ButtonActive,  UITheme::COL_BTN_ACTV);

  ui::SetNextWindowPos(pos);
  ui::SetNextWindowSize(ImVec2(winW, winH));

  bool open = true;
  ui::Begin("##gametimemenu", &open,
            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDecoration);

  const float speedFactor = GameClock::instance().getGameClockSpeed();

  struct SpeedBtn
  {
    const char *label;
    float       speed;
  };

  constexpr SpeedBtn speeds[] = {
      {"||", 0.f},
      {" >", 1.f},
      {">>", 2.f},
      {">>|", 8.f},
  };

  ui::SetCursorPosY(ui::GetCursorPosY() + 2.f);
  for (int i = 0; i < numBtns; ++i)
  {
    bool active = (fabs(speedFactor - speeds[i].speed) < 0.1f);

    if (active)
    {
      // Highlighted pressed-state colour for the active speed
      ui::PushStyleColor(ImGuiCol_Button, UITheme::COL_ACCENT_ACTIVE);
      ui::PushStyleColor(ImGuiCol_Text,   ImVec4(1.f, 1.f, 1.f, 1.f));
    }
    else
    {
      ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
    }

    ImGuiButtonFlags flags = active ? ImGuiButtonFlags_ForcePressed : 0;
    if (ui::ButtonCtEx(speeds[i].label, buttonSize, flags))
    {
      GameClock::instance().setGameClockSpeed(speeds[i].speed);
    }

    if (active)
      ui::PopStyleColor(2);
    else
      ui::PopStyleColor(1);

    if (i < numBtns - 1)
      ui::SameLine(0, spacing);
  }

  ImGui::PopStyleColor(3);
  ImGui::PopStyleVar(4);
  UITheme::popPanelStyle();

  ui::End();
}
