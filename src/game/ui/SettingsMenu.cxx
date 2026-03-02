#include "SettingsMenu.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "Settings.hxx"
#include "MapFunctions.hxx"
#include "WindowManager.hxx"

#ifdef USE_AUDIO
#include "../services/AudioMixer.hxx"
#endif

namespace ui = ImGui;

SettingsMenu::SettingsMenu() {}

void SettingsMenu::draw() const
{
  ImVec2 windowSize(560, 560);
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();
  auto &settings = Settings::instance();

  const auto &layout = uiManager.getLayouts()["PauseMenuButtons"];
  ui::SetNextWindowPos(ImVec2((screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2));
  ui::SetNextWindowSize(windowSize);

  const float contentWidth = windowSize.x - 48.f;
  const float labelColWidth = contentWidth * 0.40f;
  const float controlColWidth = contentWidth * 0.58f;

  ui::PushFont(layout.font);
  ui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
  ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  bool open = true;
  ui::BeginCt("SettingsMenu", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  {
    auto textWidth = ui::CalcTextSize("Settings").x;
    ui::SetCursorPosX((windowSize.x - textWidth) * 0.5f);
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
    ui::Text("Settings");
    ui::PopStyleColor();
  }

  ui::Spacing();

  auto sectionHeader = [&](const char *title)
  {
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.54f, 0.58f, 1.00f));
    ui::Text("%s", title);
    ui::PopStyleColor();
    ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
    ui::Separator();
    ui::PopStyleColor();
    ui::Spacing();
  };

  auto settingRow = [&](const char *label)
  {
    ui::Text("%s", label);
    ui::SameLine(labelColWidth);
    ui::PushItemWidth(controlColWidth);
  };

  auto endRow = []() { ui::PopItemWidth(); };

  sectionHeader("Display");

  settingRow("VSync");
  ui::CheckboxCt("##vsync", &settings.vSync);
  endRow();

  settingRow("Screen Mode");
  {
    std::vector<std::string> modes = {"WINDOWED", "BORDERLESS", "FULLSCREEN"};
    if (ui::BeginComboCt("##fullscreenmode", modes[settings.fullScreenMode].c_str()))
    {
      for (int i = 0; i < (int)modes.size(); ++i)
      {
        const bool is_selected = (settings.fullScreenMode == i);
        if (ui::Selectable(modes[i].c_str(), is_selected))
        {
          WindowManager::instance().setFullScreenMode(static_cast<FULLSCREEN_MODE>(i));
          MapFunctions::instance().refreshVisibleMap();
        }
        if (is_selected)
          ui::SetItemDefaultFocus();
      }
      ui::EndCombo();
    }
  }
  endRow();

  settingRow("Resolution");
  {
    std::string currentRes = std::to_string(settings.screenWidth) + " x " + std::to_string(settings.screenHeight);
    if (ui::BeginComboCt("##screenres", currentRes.c_str()))
    {
      for (const auto &mode : WindowManager::instance().getSupportedScreenResolutions())
      {
        std::string resolution = std::to_string(mode->w) + " x " + std::to_string(mode->h);
        const bool is_selected = (settings.screenHeight == mode->h && settings.screenWidth == mode->w);
        if (ui::Selectable(resolution.c_str(), is_selected))
        {
          WindowManager::instance().setScreenResolution(
              (int)(&mode - &WindowManager::instance().getSupportedScreenResolutions().front()));
          MapFunctions::instance().refreshVisibleMap();
        }
        if (is_selected)
          ui::SetItemDefaultFocus();
      }
      ui::EndCombo();
    }
  }
  endRow();

  ui::Spacing();
  sectionHeader("Interface");

  settingRow("Build Menu");
  {
    int buildMenuLayoutIdx = (int)uiManager.buildMenuLayout();
    std::vector<const char *> layouts = {"LEFT", "RIGHT", "TOP", "BOTTOM"};
    if (ui::BeginComboCt("##menulayout", layouts[buildMenuLayoutIdx]))
    {
      for (size_t n = 0, size = layouts.size(); n < size; n++)
      {
        const bool is_selected = ((int)buildMenuLayoutIdx == (int)n);
        if (ui::Selectable(layouts[n], is_selected))
        {
          settings.buildMenuPosition = layouts[n];
          uiManager.setBuildMenuLayout(static_cast<BUILDMENU_LAYOUT>(n));
        }
        if (is_selected)
          ui::SetItemDefaultFocus();
      }
      ui::EndCombo();
    }
  }
  endRow();

  ui::Spacing();
  sectionHeader("Audio");

  settingRow("Music");
  {
    float saveVolume = settings.musicVolume;
    ui::SliderFloatCt("##musicvol", &settings.musicVolume, 0, 1, "%.0f%%", ImGuiSliderFlags_None);
    if (saveVolume != settings.musicVolume)
    {
#ifdef USE_AUDIO
      AudioMixer::instance().setMusicVolume(settings.musicVolume);
#endif
    }
  }
  endRow();

  settingRow("Sound FX");
  {
    float saveSFX = settings.soundEffectsVolume;
    ui::SliderFloatCt("##sfxvol", &settings.soundEffectsVolume, 0, 1, "%.0f%%", ImGuiSliderFlags_None);
    if (saveSFX != settings.soundEffectsVolume)
    {
#ifdef USE_AUDIO
      AudioMixer::instance().setSoundEffectVolume(settings.soundEffectsVolume);
#endif
    }
  }
  endRow();

  const ImVec2 btnSize(140, 40);
  const float btnSpacing = 12.f;
  const float totalBtnWidth = btnSize.x * 3 + btnSpacing * 2;
  const float btnStartX = (windowSize.x - totalBtnWidth) / 2.f;

  ui::SetCursorPosY(windowSize.y - btnSize.y - 24.f);
  ui::SetCursorPosX(btnStartX);
  if (ui::ButtonCt("OK", btnSize))
  {
    uiManager.closeMenu();
  }

  ui::SameLine(0, btnSpacing);
  if (ui::ButtonCt("Cancel", btnSize))
  {
    uiManager.closeMenu();
  }

  ui::SameLine(0, btnSpacing);
  if (ui::ButtonCt("Reset", btnSize))
  {
    uiManager.closeMenu();
    Settings::instance().resetSettingsToDefaults();
  }

  ui::PopFont();
  ui::PopStyleVar(2);

  ui::End();
}

SettingsMenu::~SettingsMenu() {}