#include "SettingsMenu.hxx"
#include "UITheme.hxx"

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
SettingsMenu::~SettingsMenu() {}

// Helper: draws a left-aligned label, then advances the cursor to a fixed
// right-column position so all widgets are aligned.
static void settingsLabel(const char *label, float rightCol)
{
  ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_SECONDARY);
  ui::TextUnformatted(label);
  ui::PopStyleColor();
  ui::SameLine(rightCol);
}

void SettingsMenu::draw() const
{
  constexpr float winW = 560.f;
  constexpr float winH = 520.f;
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();
  auto &settings  = Settings::instance();

  const auto &layout = uiManager.getLayouts()["PauseMenuButtons"];

  ui::SetNextWindowPos(
      ImVec2((screenSize.x - winW) * 0.5f, (screenSize.y - winH) * 0.5f));
  ui::SetNextWindowSize(ImVec2(winW, winH));

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 20.f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, 12.f));

  bool open = true;
  ui::BeginCt("SettingsMenu", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  // ── Header ────────────────────────────────────────────────────────────────
  ui::PushFont(layout.font);
  {
    const char *title = "SETTINGS";
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

  // ── Sections ──────────────────────────────────────────────────────────────
  constexpr float rightCol  = 250.f;
  constexpr float widgetW   = winW - rightCol - 24.f;

  ui::PushItemWidth(widgetW);
  UITheme::pushButtonStyle();

  // ── Display ───────────────────────────────────────────────────────────────
  UITheme::sectionHeader("Display");

  // VSync
  {
    settingsLabel("Enable VSync", rightCol);
    ui::CheckboxCt("##vsync", &settings.vSync);
  }

  // Screen mode
  {
    settingsLabel("Screen Mode", rightCol);
    const std::vector<std::string> modes = {"Windowed", "Borderless", "Fullscreen"};
    if (ui::BeginComboCt("##fullscreenmode", modes[settings.fullScreenMode].c_str()))
    {
      for (int i = 0; i < static_cast<int>(modes.size()); ++i)
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

  // Resolution
  {
    settingsLabel("Resolution", rightCol);
    std::string curRes = std::to_string(settings.screenWidth) + " x " + std::to_string(settings.screenHeight);
    if (ui::BeginComboCt("##screenres", curRes.c_str()))
    {
      for (const auto &mode : WindowManager::instance().getSupportedScreenResolutions())
      {
        std::string res = std::to_string(mode->w) + " x " + std::to_string(mode->h);
        const bool is_selected = (settings.screenHeight == mode->h && settings.screenWidth == mode->w);
        if (ui::Selectable(res.c_str(), is_selected))
        {
          WindowManager::instance().setScreenResolution(
              static_cast<int>(&mode - &WindowManager::instance().getSupportedScreenResolutions().front()));
          MapFunctions::instance().refreshVisibleMap();
        }
        if (is_selected)
          ui::SetItemDefaultFocus();
      }
      ui::EndCombo();
    }
  }

  ui::Spacing();

  // ── Interface ─────────────────────────────────────────────────────────────
  UITheme::sectionHeader("Interface");

  // Build menu position
  {
    settingsLabel("Build Menu Position", rightCol);
    int buildMenuIdx = static_cast<int>(uiManager.buildMenuLayout());
    const std::vector<const char *> layouts = {"Left", "Right", "Top", "Bottom"};
    if (ui::BeginComboCt("##menulayout", layouts[buildMenuIdx]))
    {
      for (size_t n = 0; n < layouts.size(); ++n)
      {
        const bool is_selected = (buildMenuIdx == static_cast<int>(n));
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

  ui::Spacing();

  // ── Audio ─────────────────────────────────────────────────────────────────
  UITheme::sectionHeader("Audio");

  // Music volume
  {
    settingsLabel("Music Volume", rightCol);
    float saveVol = settings.musicVolume;
    ui::PushStyleColor(ImGuiCol_SliderGrab,        UITheme::COL_ACCENT);
    ui::PushStyleColor(ImGuiCol_SliderGrabActive,  UITheme::COL_ACCENT_ACTIVE);
    ui::SliderFloatCt("##musicvol", &settings.musicVolume, 0.f, 1.f, "", ImGuiSliderFlags_NoText);
    ui::PopStyleColor(2);
    if (saveVol != settings.musicVolume)
    {
#ifdef USE_AUDIO
      AudioMixer::instance().setMusicVolume(settings.musicVolume);
#endif
    }
  }

  // SFX volume
  {
    settingsLabel("Sound FX Volume", rightCol);
    float saveSFX = settings.soundEffectsVolume;
    ui::PushStyleColor(ImGuiCol_SliderGrab,       UITheme::COL_ACCENT);
    ui::PushStyleColor(ImGuiCol_SliderGrabActive, UITheme::COL_ACCENT_ACTIVE);
    ui::SliderFloatCt("##sfxvol", &settings.soundEffectsVolume, 0.f, 1.f, "", ImGuiSliderFlags_NoText);
    ui::PopStyleColor(2);
    if (saveSFX != settings.soundEffectsVolume)
    {
#ifdef USE_AUDIO
      AudioMixer::instance().setSoundEffectVolume(settings.soundEffectsVolume);
#endif
    }
  }

  ui::PopItemWidth();

  // ── Footer buttons ────────────────────────────────────────────────────────
  const float btnY  = winH - 70.f;
  const float btnW  = 130.f;
  const float gap   = 10.f;
  const float totalW = btnW * 3 + gap * 2;
  const float startX = (winW - totalW) * 0.5f;

  ui::SetCursorPos(ImVec2(startX, btnY));

  if (ui::ButtonCt("OK", ImVec2(btnW, 40.f)))
    uiManager.closeMenu();

  ui::SameLine(0.f, gap);
  if (ui::ButtonCt("Cancel", ImVec2(btnW, 40.f)))
    uiManager.closeMenu();

  ui::SameLine(0.f, gap);
  ui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.35f, 0.25f, 0.08f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.38f, 0.10f, 1.f));
  ui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.28f, 0.18f, 0.05f, 1.f));
  if (ui::ButtonCt("Reset", ImVec2(btnW, 40.f)))
  {
    uiManager.closeMenu();
    Settings::instance().resetSettingsToDefaults();
  }
  ui::PopStyleColor(3);

  UITheme::popButtonStyle();
  ImGui::PopStyleVar(2);
  UITheme::popPanelStyle();

  ui::End();
}
