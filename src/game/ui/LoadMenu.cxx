#include "LoadMenu.hxx"
#include "UITheme.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "Settings.hxx"
#include "MapFunctions.hxx"
#include "Constants.hxx"

#include <filesystem>

#ifdef USE_AUDIO
#include "../services/AudioMixer.hxx"
#endif

namespace fs = std::filesystem;
namespace ui = ImGui;

LoadMenu::LoadMenu() {}

void LoadMenu::draw() const
{
  m_result = e_none;
  m_filename.clear();

  constexpr float winW = 460.f;
  constexpr float winH = 460.f;
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();
  const auto &layout = uiManager.getLayouts()["LoadDialogButtons"];

  ui::SetNextWindowPos(
      ImVec2((screenSize.x - winW) * 0.5f, (screenSize.y - winH) * 0.5f));
  ui::SetNextWindowSize(ImVec2(winW, winH));

  UITheme::pushPanelStyle();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20.f, 18.f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,   ImVec2(0.f, 8.f));

  bool open = true;
  ui::BeginCt("LoadDialog", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  // ── Header ────────────────────────────────────────────────────────────────
  ui::PushFont(layout.font);
  {
    const char *title = "LOAD CITY";
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

  // ── Save file list ────────────────────────────────────────────────────────
  const float listH    = winH - 160.f;
  const float deleteW  = 32.f;
  const float entryW   = winW - 40.f - deleteW - 6.f;

  UITheme::pushButtonStyle();

  ui::BeginChild("##loadbuttons", ImVec2(winW - 40.f, listH), false,
                 ImGuiWindowFlags_NoTitleBar);

  std::string path = CYTOPIA_DATA_DIR + std::string("/saves");
  bool any = false;
  for (const auto &entry : fs::directory_iterator(path))
  {
    if (!fs::is_regular_file(entry) || entry.path().extension() != ".cts")
      continue;

    any = true;
    const std::string name = entry.path().filename().string();

    // Load button
    if (ui::ButtonCt(name.c_str(), ImVec2(entryW, 36.f)))
    {
      m_filename = name;
      m_result   = e_load_file;
    }

    // Delete button — red tint
    ui::SameLine(0.f, 6.f);
    ui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.55f, 0.10f, 0.10f, 1.f));
    ui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.75f, 0.15f, 0.15f, 1.f));
    ui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.08f, 0.08f, 1.f));
    std::string delId = "X##del_" + name;
    if (ui::ButtonCt(delId.c_str(), ImVec2(deleteW, 36.f)))
    {
      m_filename = name;
      m_result   = e_delete_file;
    }
    if (ui::IsItemHovered())
      ui::SetTooltip("Permanently delete save file");
    ui::PopStyleColor(3);
  }

  if (!any)
  {
    ui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
    float tw = ui::CalcTextSize("No save files found.").x;
    ui::SetCursorPosX(((winW - 40.f) - tw) * 0.5f);
    ui::TextUnformatted("No save files found.");
    ui::PopStyleColor();
  }

  ui::EndChild();

  // ── Footer ────────────────────────────────────────────────────────────────
  ui::Spacing();
  ui::Separator();
  ui::Spacing();

  const float btnW  = 140.f;
  ui::SetCursorPosX((winW - btnW) * 0.5f);
  if (ui::ButtonCt("Close", ImVec2(btnW, 38.f)))
    m_result = e_close;

  UITheme::popButtonStyle();
  ImGui::PopStyleVar(2);
  UITheme::popPanelStyle();

  ui::End();
}

LoadMenu::~LoadMenu() {}
