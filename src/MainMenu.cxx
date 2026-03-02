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

  int screenWidth = Settings::instance().screenWidth;
  int screenHeight = Settings::instance().screenHeight;
  bool mainMenuLoop = true;
  bool startGame = true;
  bool showLoadDialog = false;

  UIManager::instance().init();

#ifdef USE_AUDIO
  auto &m_AudioMixer = AudioMixer::instance();
  /* Trigger MainMenu music */
  m_AudioMixer.setMusicVolume(Settings::instance().musicVolume);
  m_AudioMixer.setSoundEffectVolume(Settings::instance().soundEffectsVolume); // does nothing right now
  if (!Settings::instance().audio3DStatus)
    m_AudioMixer.play(AudioTrigger::MainMenu);
  else
    m_AudioMixer.play(AudioTrigger::MainMenu, Coordinate3D{0, 3, 0.5});
#endif // USE_AUDIO

  auto logoTex = ResourcesManager::instance().getUITexture("Cytopia_Logo");
  auto discordTex = ResourcesManager::instance().getUITexture("Discord_icon");
  auto githubTex = ResourcesManager::instance().getUITexture("Github_icon");
  int logoTexW, logoTexH;
  SDL_QueryTexture(logoTex, nullptr, nullptr, &logoTexW, &logoTexH);

  auto beginFrame = []
  {
    SDL_RenderClear(WindowManager::instance().getRenderer());

    WindowManager::instance().newImGuiFrame();

    ui::SetNextWindowBgAlpha(0);
    ui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ui::SetNextWindowSize(ui::GetIO().DisplaySize);

    bool open = true;
    ui::Begin("MainWnd", &open,
              ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar |
                  ImGuiWindowFlags_NoScrollWithMouse);
  };

  auto renderFrame = []
  {
    ui::End();

    WindowManager::instance().renderScreen();
    SDL_Delay(5);
  };

  // fade in Logo
  const ImVec2 logoImgPos(screenWidth / 2 - logoTexW / 2, screenHeight / 4);
  for (Uint8 opacity = 0; opacity < 255; opacity++)
  {
    beginFrame();

    // break the loop if an event occurs
    const bool has_event = SDL_PollEvent(&event) != 0;
    if (has_event && event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_KEYDOWN)
      opacity = 254;

    ui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ui::SetCursorPos(logoImgPos);
    float op = opacity / 255.f;
    ui::Image(logoTex, ImVec2(logoTexW, logoTexH), ImVec2(0, 0), ImVec2(1, 1), ImVec4{op, op, op, op});
    ui::PopStyleVar(1);

    renderFrame();
  }

  LoadMenu loadMenu;

  const auto &buttonFont = UIManager::instance().getLayouts()["MainMenuButtons"].font;
  while (mainMenuLoop)
  {
    beginFrame();

    while (SDL_PollEvent(&event) != 0)
    {
      ImGui_ImplSDL2_ProcessEvent(&event);

      if (event.type == SDL_QUIT)
      {
        startGame = false;
        mainMenuLoop = false;
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
      ui::PushFont(buttonFont);

      ui::SetCursorPos(logoImgPos);
      ui::Image(logoTex, ImVec2(logoTexW, logoTexH));

      const ImVec2 taglineSize = ui::CalcTextSize("Build the city of your dreams");
      ImVec2 taglinePos(screenWidth / 2 - taglineSize.x / 2, logoImgPos.y + logoTexH + 8);
      ui::SetCursorPos(taglinePos);
      ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.54f, 0.58f, 1.00f));
      ui::Text("Build the city of your dreams");
      ui::PopStyleColor();

      constexpr ImVec2 buttonSize(260, 48);
      constexpr int buttonInterval = 14;
      float menuStartY = taglinePos.y + taglineSize.y + 40;
      ImVec2 buttonPos(screenWidth / 2.f - buttonSize.x / 2.f, menuStartY);

      ui::SetCursorPos(buttonPos);
      if (ui::ButtonCt("New Game", buttonSize))
      {
#ifdef USE_AUDIO
        playAudioMajorSelection();
#endif
        mainMenuLoop = false;
        SignalMediator::instance().signalNewGame.emit(true);
      }

      buttonPos.y += buttonSize.y + buttonInterval;
      ui::SetCursorPos(buttonPos);
      if (ui::ButtonCt("Load Game", buttonSize))
      {
        showLoadDialog = true;
      }

      buttonPos.y += buttonSize.y + buttonInterval;
      ui::SetCursorPos(buttonPos);
      if (ui::ButtonCt("Quit Game", buttonSize))
      {
        startGame = false;
        mainMenuLoop = false;
      }

      constexpr int xOffset = 14, btnSize = 36;
      ImVec2 leftBottom(xOffset, screenHeight - btnSize - xOffset);
      ui::SetCursorPos(leftBottom);
      if (ui::ImageButton(discordTex, ImVec2(btnSize, btnSize)))
      {
        OSystem::openDir("https://discord.gg/MG3tgYV6ce");
      }

      leftBottom.x += xOffset + btnSize + 6;
      ui::SetCursorPos(leftBottom);
      if (ui::ImageButton(githubTex, ImVec2(btnSize, btnSize)))
      {
        OSystem::openDir("https://github.com/CytopiaTeam/Cytopia/issues/new");
      }

      ui::PopFont();
    }

    ui::PushStyleColor(ImGuiCol_Text, ImVec4(0.50f, 0.54f, 0.58f, 0.70f));
    constexpr ImVec2 fpsTextPos(12, 8);
    ui::SetCursorPos(fpsTextPos);
    ui::Text("[%.0f FPS]", ui::GetIO().Framerate);

    ImVec2 textSize = ImGui::CalcTextSize(VERSION);
    const ImVec2 versionPos(screenWidth - textSize.x - 14, screenHeight - textSize.y - 12);
    ui::SetCursorPos(versionPos);
    ui::Text(VERSION);
    ui::PopStyleColor();

    renderFrame();
  }
  return startGame;
}