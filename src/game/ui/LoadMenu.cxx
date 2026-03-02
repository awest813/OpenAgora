#include "LoadMenu.hxx"

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

  ImVec2 windowSize(480, 480);
  ImVec2 screenSize = ui::GetIO().DisplaySize;

  auto &uiManager = UIManager::instance();

  const auto &layout = uiManager.getLayouts()["LoadDialogButtons"];
  ui::SetNextWindowPos(ImVec2((screenSize.x - windowSize.x) / 2, (screenSize.y - windowSize.y) / 2));
  ui::SetNextWindowSize(windowSize);

  const float margin = 24.f;
  constexpr float btnSide = 36;
  const ImVec2 buttonSize(windowSize.x - margin * 2 - btnSide - 10, 40);

  ui::PushFont(layout.font);
  ui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(margin, 20));
  ui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 10));

  bool open = true;
  ui::BeginCt("LoadDialog", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);

  {
    auto textWidth = ui::CalcTextSize("Load City").x;
    ui::SetCursorPosX((windowSize.x - textWidth) * 0.5f);
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.30f, 0.78f, 0.74f, 1.00f));
    ui::Text("Load City");
    ui::PopStyleColor();
  }

  ui::Spacing();
  ui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.22f, 0.28f, 0.36f, 0.40f));
  ui::Separator();
  ui::PopStyleColor();
  ui::Spacing();

  ui::BeginChild("##loadbuttons", {0.f, windowSize.y - 160.f}, false,
                 ImGuiWindowFlags_NoTitleBar);

  std::string path = CYTOPIA_DATA_DIR + "/saves";
  for (const auto &entry : fs::directory_iterator(path))
  {
    if (!fs::is_regular_file(entry) || entry.path().extension() != ".cts")
      continue;

    if (ui::ButtonCt(entry.path().filename().string().c_str(), buttonSize))
    {
      m_filename = entry.path().filename().string();
      m_result = e_load_file;
    }

    ui::SameLine();
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.40f, 0.35f, 1.00f));
    if (ui::ButtonCt("X", {btnSide, btnSide}))
    {
      m_filename = entry.path().filename().string();
      m_result = e_delete_file;
    }
    ui::PopStyleColor();
  }

  ui::EndChild();

  const ImVec2 closeBtnSize(160, 40);
  ui::SetCursorPosY(windowSize.y - closeBtnSize.y - 24.f);
  ui::SetCursorPosX((windowSize.x - closeBtnSize.x) / 2.f);
  if (ui::ButtonCt("Close", closeBtnSize))
  {
    m_result = e_close;
  }

  ui::PopFont();
  ui::PopStyleVar(2);

  ui::End();
}

LoadMenu::~LoadMenu() {}