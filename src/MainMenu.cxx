#include "MainMenu.hxx"
#ifdef USE_AUDIO
#include "services/AudioMixer.hxx"
#endif
#include <Settings.hxx>
#include <SignalMediator.hxx>
#include <SDL.h>
#include "engine/UIManager.hxx"
#include "engine/basics/Settings.hxx"
#include "../engine/ResourcesManager.hxx"
#include "WindowManager.hxx"
#include "OSystem.hxx"

#include "imgui.h"
#include "imgui_impl_sdl.h"
#include "imgui_impl_sdlrenderer.h"

#include "game/ui/LoadMenu.hxx"
#include "game/ui/UITheme.hxx"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"

#include <cmath>
#include <cstdio>

namespace ui = ImGui;

#ifdef USE_AUDIO
static void playAudioMajorSelection()
{
  auto &m_AudioMixer = AudioMixer::instance();
  m_AudioMixer.stopAll();
  if (!Settings::instance().audio3DStatus)
    m_AudioMixer.play(SoundtrackID{"MajorSelection"});
  else
    m_AudioMixer.play(SoundtrackID{"MajorSelection"}, Coordinate3D{0, 0, -4});
}
#endif // USE_AUDIO

bool mainMenu()
{
  SDL_Event event;

  const int screenWidth  = Settings::instance().screenWidth;
  const int screenHeight = Settings::instance().screenHeight;

  bool mainMenuLoop = true;
  bool startGame    = true;
  bool showLoadDialog = false;

  UIManager::instance().init();

#ifdef USE_AUDIO
  auto &m_AudioMixer = AudioMixer::instance();
  m_AudioMixer.setMusicVolume(Settings::instance().musicVolume);
  m_AudioMixer.setSoundEffectVolume(Settings::instance().soundEffectsVolume);
  if (!Settings::instance().audio3DStatus)
    m_AudioMixer.play(AudioTrigger::MainMenu);
  else
    m_AudioMixer.play(AudioTrigger::MainMenu, Coordinate3D{0, 3, 0.5});
#endif

  auto logoTex    = ResourcesManager::instance().getUITexture("Cytopia_Logo");
  auto discordTex = ResourcesManager::instance().getUITexture("Discord_icon");
  auto githubTex  = ResourcesManager::instance().getUITexture("Github_icon");
  int logoTexW = 0, logoTexH = 0;
  SDL_QueryTexture(logoTex, nullptr, nullptr, &logoTexW, &logoTexH);

  // ── Frame helpers ─────────────────────────────────────────────────────────
  auto beginFrame = []
  {
    SDL_RenderClear(WindowManager::instance().getRenderer());
    WindowManager::instance().newImGuiFrame();
    ui::SetNextWindowBgAlpha(0.f);
    ui::SetNextWindowPos(ImVec2(0.f, 0.f));
    ui::SetNextWindowSize(ui::GetIO().DisplaySize);
    bool open = true;
    ui::Begin("MainWnd", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                  ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);
  };

  auto renderFrame = []
  {
    ui::End();
    WindowManager::instance().renderScreen();
    SDL_Delay(5);
  };

  // ── Background gradient helper ────────────────────────────────────────────
  // Draws a vertical gradient across the full window background each frame.
  auto drawBackground = [&]()
  {
    ImDrawList *dl = ui::GetWindowDrawList();
    const ImVec2 &sz = ui::GetIO().DisplaySize;
    // Deep navy → slightly lighter at the bottom
    dl->AddRectFilledMultiColor(
        ImVec2(0, 0), sz,
        IM_COL32(10,  13,  22,  255),   // top-left
        IM_COL32(10,  13,  22,  255),   // top-right
        IM_COL32(18,  24,  40,  255),   // bottom-right
        IM_COL32(18,  24,  40,  255));  // bottom-left
  };

  // ── Logo fade-in ──────────────────────────────────────────────────────────
  // Scale logo to at most 50 % of screen width
  const float maxLogoW = screenWidth * 0.5f;
  float scaledLogoW = (float)logoTexW;
  float scaledLogoH = (float)logoTexH;
  if (scaledLogoW > maxLogoW)
  {
    float ratio  = maxLogoW / scaledLogoW;
    scaledLogoW  = maxLogoW;
    scaledLogoH *= ratio;
  }

  const ImVec2 logoPos(
      (screenWidth  - scaledLogoW) * 0.5f,
      screenHeight  * 0.18f);

  for (Uint8 opacity = 0; opacity < 255; opacity += 3)
  {
    beginFrame();
    drawBackground();

    const bool has_event = SDL_PollEvent(&event) != 0;
    if ((has_event && event.type == SDL_MOUSEBUTTONDOWN) || event.type == SDL_KEYDOWN)
      opacity = 252; // jump to end

    float op = (float)opacity / 255.f;
    ui::SetCursorPos(logoPos);
    ui::Image(logoTex,
              ImVec2(scaledLogoW, scaledLogoH),
              ImVec2(0, 0), ImVec2(1, 1),
              ImVec4{op, op, op, op});

    renderFrame();
  }

  // ── Main loop ─────────────────────────────────────────────────────────────
  LoadMenu loadMenu;
  const auto &buttonFont = UIManager::instance().getLayouts()["MainMenuButtons"].font;

  // Animated pulse time for button highlight
  float animTime = 0.f;

  while (mainMenuLoop)
  {
    animTime += ui::GetIO().DeltaTime;
    beginFrame();
    drawBackground();

    while (SDL_PollEvent(&event) != 0)
    {
      ImGui_ImplSDL2_ProcessEvent(&event);
      if (event.type == SDL_QUIT)
      {
        startGame     = false;
        mainMenuLoop  = false;
      }
    }

    if (showLoadDialog)
    {
      loadMenu.draw();
      switch (loadMenu.result())
      {
      case LoadMenu::e_close:
        showLoadDialog = false;
        break;

      case LoadMenu::e_load_file:
#ifdef USE_AUDIO
        playAudioMajorSelection();
#endif
        SignalMediator::instance().signalLoadGame.emit(loadMenu.filename());
        mainMenuLoop = false;
        break;

      default:
        break;
      }
    }
    else
    {
      // ── Logo ──────────────────────────────────────────────────────────────
      ui::SetCursorPos(logoPos);
      ui::Image(logoTex, ImVec2(scaledLogoW, scaledLogoH));

      // ── Menu card ─────────────────────────────────────────────────────────
      constexpr float cardW   = 280.f;
      constexpr float btnH    = 44.f;
      constexpr float btnGap  = 10.f;
      constexpr int   numBtns = 3;
      constexpr float cardPad = 28.f;
      const float     cardH   = cardPad * 2 + btnH * numBtns + btnGap * (numBtns - 1);
      const float     cardX   = (screenWidth  - cardW) * 0.5f;
      const float     cardY   = logoPos.y + scaledLogoH + screenHeight * 0.06f;

      // Draw semi-transparent card background
      {
        ImDrawList *dl = ui::GetWindowDrawList();
        const ImVec2 cardMin(cardX - 2.f, cardY - 2.f);
        const ImVec2 cardMax(cardX + cardW + 2.f, cardY + cardH + 2.f);

        // Outer glow / border
        dl->AddRectFilled(cardMin, cardMax,
                          IM_COL32(40, 90, 160, 60), 10.f);
        // Card body
        dl->AddRectFilled(
            ImVec2(cardX, cardY),
            ImVec2(cardX + cardW, cardY + cardH),
            IM_COL32(12, 16, 26, 215), 8.f);
        // Top accent line
        dl->AddLine(ImVec2(cardX + 10.f, cardY + 1.f),
                    ImVec2(cardX + cardW - 10.f, cardY + 1.f),
                    IM_COL32(80, 160, 255, 180), 2.f);
      }

      // Buttons
      ui::PushFont(buttonFont);

      // Style vars for menu buttons
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.f);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,  ImVec2(10.f, 8.f));
      ImGui::PushStyleColor(ImGuiCol_Button,        UITheme::COL_BTN);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, UITheme::COL_BTN_HOVER);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  UITheme::COL_BTN_ACTV);
      ImGui::PushStyleColor(ImGuiCol_Text,          UITheme::COL_TEXT_PRIMARY);

      const ImVec2 btnSize(cardW - cardPad * 2, btnH);
      float btnY = cardY + cardPad;
      const float btnX = cardX + cardPad;

      auto drawBtn = [&](const char *label, const char *tooltip = nullptr, bool destructive = false) -> bool {
        if (destructive)
        {
          ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.28f, 0.06f, 0.06f, 1.f));
          ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.48f, 0.10f, 0.10f, 1.f));
          ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.22f, 0.05f, 0.05f, 1.f));
        }
        ui::SetCursorPos(ImVec2(btnX, btnY));
        btnY += btnH + btnGap;
        bool clicked = ui::ButtonCt(label, btnSize);
        if (tooltip && ui::IsItemHovered())
          ui::SetTooltip("%s", tooltip);
        if (destructive)
          ImGui::PopStyleColor(3);
        return clicked;
      };

      if (drawBtn("New Game", "Start a new city"))
      {
#ifdef USE_AUDIO
        playAudioMajorSelection();
#endif
        mainMenuLoop = false;
        SignalMediator::instance().signalNewGame.emit(true);
      }

      if (drawBtn("Load Game", "Load a saved city"))
        showLoadDialog = true;

      if (drawBtn("Quit Game", "Exit to desktop", /*destructive=*/true))
      {
        startGame    = false;
        mainMenuLoop = false;
      }

      ImGui::PopStyleColor(4);
      ImGui::PopStyleVar(2);
      ui::PopFont();

      // ── Social icon buttons ───────────────────────────────────────────────
      constexpr int xOff = 8, iconSz = 32;
      const float iconY = (float)(screenHeight - iconSz - xOff * 2);

      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.f);
      ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.f, 0.f, 0.f, 0.f));
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.f, 1.f, 1.f, 0.12f));
      ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.f, 1.f, 1.f, 0.20f));

      ui::SetCursorPos(ImVec2((float)xOff, iconY));
      if (ui::ImageButton(discordTex, ImVec2(iconSz, iconSz)))
        OSystem::openDir("https://discord.gg/MG3tgYV6ce");

      ui::SetCursorPos(ImVec2((float)(xOff * 3 + iconSz), iconY));
      if (ui::ImageButton(githubTex, ImVec2(iconSz, iconSz)))
        OSystem::openDir("https://github.com/CytopiaTeam/Cytopia/issues/new");

      ImGui::PopStyleColor(3);
      ImGui::PopStyleVar(1);
    }

    // ── FPS + version overlay ─────────────────────────────────────────────
    {
      ImGui::PushStyleColor(ImGuiCol_Text, UITheme::COL_TEXT_DISABLED);
      ui::SetCursorPos(ImVec2(6.f, 6.f));
      ui::Text("%.0f FPS", ui::GetIO().Framerate);

      const char *ver = VERSION;
      ImVec2 textSize = ui::CalcTextSize(ver);
      ui::SetCursorPos(ImVec2((float)screenWidth - textSize.x - 8.f,
                              (float)screenHeight - textSize.y - 6.f));
      ui::TextUnformatted(ver);
      ImGui::PopStyleColor();
    }

    renderFrame();
  }

  return startGame;
}
