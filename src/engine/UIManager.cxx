#include "UIManager.hxx"

#include "Constants.hxx"
#include "ResourcesManager.hxx"
#include <MapFunctions.hxx>
#include "Map.hxx"
#include "basics/mapEdit.hxx"
#include "basics/Settings.hxx"
#include "basics/utils.hxx"
#include "LOG.hxx"
#include "Exception.hxx"
#include "GameStates.hxx"
#include "MapLayers.hxx"
#include "Filesystem.hxx"
#include "json.hxx"
#include "betterEnums.hxx"
#include <Camera.hxx>
#include "../services/FrameMetrics.hxx"

#ifdef USE_AUDIO
#include "../services/AudioMixer.hxx"
#endif

#include <cmath>
#include <array>
#include <cstdio>

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#ifdef MICROPROFILE_ENABLED
#include "microprofile/microprofile.h"
#endif

namespace ui = ImGui;

BETTER_ENUM(Action, int, RaiseTerrain, LowerTerrain, QuitGame, Demolish, ChangeTileType, ToggleVisibilityOfGroup, NewGame,
            SaveGame, LoadGame, SaveSettings, ChangeResolution)

void UIManager::init()
{
  json uiLayout;

  loadSettings(uiLayout);
  parseLayouts(uiLayout);

  initializeImGuiFonts();
}

ImFont *UIManager::loadFont(const std::string &fontPath, uint32_t size)
{
  auto *uiFonts = ImGui::GetIO().Fonts;
  if (!fontDefault)
    fontDefault = uiFonts->AddFontDefault();

  std::string hashName = fontPath;
  hashName.append(std::to_string(size));
  if (const auto it = m_loadedFonts.find(hashName); it != m_loadedFonts.end())
    return it->second;

  ImFont *newFont = uiFonts->AddFontFromFileTTF(fontPath.c_str(), (float)size);
  m_loadedFonts[hashName] = newFont;
  uiFonts->Build();

  return newFont;
}

void UIManager::initializeImGuiFonts()
{
  std::string fontPath = fs::getBasePath() + Settings::instance().fontFileName.get(); // fix for macos, need to use abs path

  const auto names = {"MainMenuButtons", "PauseMenuButtons", "LoadDialogButtons", "BuildMenuButtons"};
  for (const auto &name : names)
  {
    if (const auto it = m_layouts.find(name); it != m_layouts.end())
    {
      auto &layout = it->second;
      layout.font = loadFont(fontPath, layout.fontSize);
    }
  }
}

void UIManager::loadSettings(json &uiLayout)
{
  std::string jsonFileContent = fs::readFileAsString(Settings::instance().uiLayoutJSONFile.get());
  uiLayout = json::parse(jsonFileContent, nullptr, false);

  // check if json file can be parsed
  if (uiLayout.is_discarded())
    throw ConfigurationError(TRACE_INFO "Error parsing JSON File " + Settings::instance().uiLayoutJSONFile.get());
}

void UIManager::parseLayouts(const json &uiLayout)
{
  // parse Layout
  for (const auto &it : uiLayout["LayoutGroups"].items())
  {
    std::string layoutGroupName;

    // prepare empty layout groups with layout information from json
    for (size_t id = 0; id < uiLayout["LayoutGroups"][it.key()].size(); id++)
    {
      layoutGroupName = uiLayout["LayoutGroups"][it.key()][id].value("GroupName", "");

      if (!layoutGroupName.empty())
      {
        LayoutData layout;
        layout.layoutType = uiLayout["LayoutGroups"][it.key()][id].value("LayoutType", "");
        layout.alignment = uiLayout["LayoutGroups"][it.key()][id].value("Alignment", "");

        if (layout.layoutType.empty())
        {
          LOG(LOG_WARNING) << "Skipping LayoutGroup " << layoutGroupName
                           << " because it has no parameter \"LayoutType\" set. Check your UiLayout.json file.";
          continue;
        }

        if (layout.alignment.empty())
        {
          LOG(LOG_WARNING) << "Skipping LayoutGroup " << layoutGroupName
                           << " because it has no parameter \"Alignment\" set. Check your UiLayout.json file.";
          continue;
        }

        layout.fontSize = uiLayout["LayoutGroups"][it.key()][id].value("FontSize", Settings::instance().defaultFontSize);
        layout.padding = uiLayout["LayoutGroups"][it.key()][id].value("Padding", 0);
        layout.paddingToParent = uiLayout["LayoutGroups"][it.key()][id].value("PaddingToParent", 0);
        layout.alignmentOffset = uiLayout["LayoutGroups"][it.key()][id].value("AlignmentOffset", 0.0F);

        // add layout group information to container
        m_layouts[layoutGroupName] = layout;
      }
      else
      {
        LOG(LOG_WARNING) << "Cannot add a Layout Group without a name. Check your UiLayout.json file.";
      }
    }
  }
}

void UIManager::setFPSCounterText(const std::string &fps) { m_fpsCounter = fps; }

void UIManager::closeOpenMenus()
{
  for (auto &m : m_persistentMenu)
  {
    m->closeSubmenus();
  }
}

void UIManager::openMenu(GameMenu::Ptr menu)
{
  if (std::none_of(std::begin(m_menuStack), std::end(m_menuStack), [&](const auto &m) { return m == menu; }))
  {
    m_menuStack.push_back(menu);

    Camera::instance().canScale(false);
    Camera::instance().canMove(false);
  }
}

void UIManager::closeMenu()
{
  if (m_menuStack.empty())
    return;

  m_menuStack.pop_back();

  Camera::instance().canScale(m_menuStack.empty());
  Camera::instance().canMove(m_menuStack.empty());
}

void UIManager::addPersistentMenu(GameMenu::Ptr menu)
{
  if (std::none_of(std::begin(m_persistentMenu), std::end(m_persistentMenu), [&](const auto &m) { return m == menu; }))
  {
    m_persistentMenu.push_back(menu);
  }
}

bool UIManager::isMouseHovered() const { return ImGui::IsAnyItemHovered(); }

void UIManager::drawUI()
{
#ifdef MICROPROFILE_ENABLED
  MICROPROFILE_SCOPEI("UI", "draw UI", MP_BLUE);
#endif
  if (!m_menuStack.empty())
  {
    m_menuStack.back()->draw();
  }

  for (const auto &m : m_persistentMenu)
  {
    m->draw();
  }

  if (!m_tooltip.empty())
  {
    ImVec2 pos = ui::GetMousePos();
    ui::SetCursorScreenPos(pos);
    ui::Text("%s", m_tooltip.c_str());
  }

  if (m_showFpsCounter)
  {
    ui::SetNextWindowPos(ImVec2(0, 0));
    ui::SetNextWindowSize(ImVec2(140, 20));

    bool open = true;
    ui::Begin("##fpswindow", &open,
              ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);
    ui::Text("%s", m_fpsCounter.c_str());
    ui::SameLine();
    ui::Checkbox("debug", &m_showDebugMenu);
    ui::End();
  }

  // ── Developer debug overlay (toggle: F3 or the "debug" checkbox) ──────────
  if (m_showDebugMenu)
  {
    const FrameMetrics &fm = FrameMetrics::instance();

    // Anchor to the top-right corner with a fixed width.
    const ImGuiIO &io = ui::GetIO();
    constexpr float overlayW = 280.f;
    constexpr float overlayH = 200.f;
    constexpr float margin   = 10.f;
    ui::SetNextWindowPos(ImVec2(io.DisplaySize.x - overlayW - margin, margin), ImGuiCond_Always);
    ui::SetNextWindowSize(ImVec2(overlayW, overlayH), ImGuiCond_Always);
    ui::SetNextWindowBgAlpha(0.80f);

    bool dbgOpen = true;
    ui::Begin("##debug_overlay", &dbgOpen,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar |
                  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings |
                  ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoFocusOnAppearing |
                  ImGuiWindowFlags_NoNav);

    // Header
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.85f, 1.00f, 1.00f));
    ui::TextUnformatted("  \xe2\x9a\x99 Performance Debug Overlay");
    ui::PopStyleColor();
    ui::Separator();

    // Thresholds and colours for the frame-time indicator.
    // 60 FPS = 16.7 ms/frame; 30 FPS = 33.3 ms/frame.
    constexpr float TARGET_60FPS_MS    = 16.7f;
    constexpr float TARGET_30FPS_MS    = 33.3f;
    // UI is expected to consume at most ~1/6th of the total frame budget,
    // so multiply uiMs by 6 before passing to frameColour to get a comparable scale.
    constexpr float UI_BUDGET_SCALE    = 6.f;
    constexpr ImVec4 COL_PERF_GOOD     = {0.24f, 0.82f, 0.40f, 1.00f}; // green
    constexpr ImVec4 COL_PERF_WARN     = {0.97f, 0.80f, 0.20f, 1.00f}; // yellow
    constexpr ImVec4 COL_PERF_BAD      = {0.95f, 0.28f, 0.22f, 1.00f}; // red
    constexpr ImVec4 COL_LABEL         = {0.60f, 0.65f, 0.72f, 1.00f}; // muted

    // Metrics rows
    const auto printRow = [&COL_LABEL](const char *label, float valueMs, ImVec4 colour)
    {
      ui::PushStyleColor(ImGuiCol_Text, COL_LABEL);
      ui::Text("  %-10s", label);
      ui::PopStyleColor();
      ui::SameLine(120.f);
      ui::PushStyleColor(ImGuiCol_Text, colour);
      char buf[32];
      std::snprintf(buf, sizeof(buf), "%.2f ms", valueMs);
      ui::TextUnformatted(buf);
      ui::PopStyleColor();
    };

    // Returns a colour indicating whether a frame-time is within budget.
    const auto frameColour = [&](float ms) -> ImVec4
    {
      if (ms < TARGET_60FPS_MS) return COL_PERF_GOOD;
      if (ms < TARGET_30FPS_MS) return COL_PERF_WARN;
      return COL_PERF_BAD;
    };

    // FPS
    ui::PushStyleColor(ImGuiCol_Text, COL_LABEL);
    ui::TextUnformatted("  FPS");
    ui::PopStyleColor();
    ui::SameLine(120.f);
    ui::PushStyleColor(ImGuiCol_Text, frameColour(fm.lastFrameMs()));
    char fpsBuf[16];
    std::snprintf(fpsBuf, sizeof(fpsBuf), "%.1f", fm.avgFPS());
    ui::TextUnformatted(fpsBuf);
    ui::PopStyleColor();

    printRow("Frame",  fm.lastFrameMs(), frameColour(fm.lastFrameMs()));
    printRow("UI",     fm.lastUIMs(),    frameColour(fm.lastUIMs() * UI_BUDGET_SCALE));
    printRow("Peak",   fm.peakFrameMs(), frameColour(fm.peakFrameMs()));

    ui::Spacing();

    // Frame-time history graph
    ui::PushStyleColor(ImGuiCol_PlotLines,      ImVec4(0.30f, 0.65f, 1.00f, 1.00f));
    ui::PushStyleColor(ImGuiCol_FrameBg,        ImVec4(0.12f, 0.14f, 0.18f, 0.95f));

    const auto &hist   = fm.frameHistory();
    char graphLabel[32];
    std::snprintf(graphLabel, sizeof(graphLabel), "%.1f ms", fm.lastFrameMs());
    ui::PlotLines("##framegraph", hist.data(), FrameMetrics::HISTORY_SIZE,
                  fm.frameHistoryOffset(), graphLabel,
                  0.f, 50.f, ImVec2(-1.f, 50.f));
    ui::PopStyleColor(2);

    // Active UI state
    ui::Spacing();
    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.65f, 0.72f, 1.00f));
    ui::Text("  Menus open: %d", static_cast<int>(m_menuStack.size()));
    ui::PopStyleColor();

    ui::End();
  }
}

void UIManager::setTooltip(const std::string &tooltipText) { m_tooltip = tooltipText; }

void UIManager::clearTooltip() { m_tooltip.clear(); }
